#include "bridge.hpp"
#include "../core/log.hpp"
#include "../gen/settings.hpp"
#include <cstring>

namespace beamls
{
	bool Bridge::start(const std::string &gtaBuild, std::uint32_t session)
	{
		build_ = gtaBuild;
		session_ = session;
		port_ = settings::bridge_port;
		open_ = udp_.open();
		if (open_)
			log::info("bridge: udp 127.0.0.1:%d -> %d", udp_.localPort(), port_);
		else
			log::error("bridge: cannot open a UDP socket");
		return open_;
	}

	void Bridge::stop(const char *reason)
	{
		if (!open_)
			return;
		msg::Bye bye;
		bye.reason = reason;
		send(bye);
		udp_.close();
		open_ = false;
		acked_ = ready_ = false;
	}

	void Bridge::poll(double now)
	{
		if (!open_)
			return;
		char buf[65536];
		for (int i = 0; i < 256; ++i)
		{
			int from = 0;
			const int n = udp_.recv(buf, sizeof(buf) - 1, from);
			if (n <= 0)
				break;
			if (from != port_)
				continue; // only BeamNG's port talks to us
			buf[n] = 0;
			handle(buf, n, now);
		}
		if (acked_ && (now - lastRecv_) * 1000.0 > settings::peer_timeout_ms)
		{
			if (!timedOut_)
				log::warn("bridge: BeamNG went silent");
			timedOut_ = true;
			acked_ = ready_ = false;
			hasCar = false;
		}
		if (!acked_ && (now - lastHello_) * 1000.0 >= settings::hello_interval_ms)
		{
			lastHello_ = now;
			msg::Hello h;
			h.proto = msg::kProto;
			h.gta_build = build_;
			h.session = session_;
			send(h);
		}
	}

	void Bridge::handle(const char *data, int len, double now)
	{
		json::Value v;
		if (!json::parse(data, std::size_t(len), v) || !v.isObject())
		{
			++malformed_;
			return;
		}
		const json::Value *t = v.get("t");
		if (!t)
		{
			++malformed_;
			return;
		}
		++received_;
		lastRecv_ = now;
		timedOut_ = false;
		const std::string &type = t->str();
		if (type == msg::Car::kType)
		{
			msg::Car c;
			if (c.decode(v))
			{
				car = c;
				hasCar = true;
				carTime = now;
			}
		}
		else if (type == msg::HelloAck::kType)
		{
			msg::HelloAck a;
			a.decode(v);
			if (a.proto != msg::kProto)
			{
				log::error("bridge: BeamNG speaks protocol %d, we speak %d", a.proto, msg::kProto);
				return;
			}
			if (!acked_)
				log::info("bridge: BeamNG %s answered (ready=%d)", a.bng_version.c_str(), int(a.ready));
			acked_ = true;
			ready_ = ready_ || a.ready;
			bngVersion = a.bng_version;
		}
		else if (type == msg::Status::kType)
		{
			status.decode(v);
			if (status.state == "ready" || status.state == "menu")
				ready_ = true;
			if (status.state == "error")
				log::error("BeamNG: %s", status.msg.c_str());
		}
		else if (type == msg::VehicleInfo::kType)
		{
			msg::VehicleInfo info;
			if (info.decode(v))
				vehicleInfos.push_back(info);
		}
		else if (type == msg::MenuClosed::kType)
		{
			msg::MenuClosed m;
			if (m.decode(v))
				menusClosed.push_back(m);
		}
		else if (type == msg::Bye::kType)
		{
			byeReceived = true;
			acked_ = ready_ = false;
			hasCar = false;
		}
	}
}
