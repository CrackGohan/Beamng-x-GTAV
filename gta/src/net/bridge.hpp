// GTA's end of the bridge: one UDP socket to BeamNG on 127.0.0.1:settings.bridge_port. State messages keep
// only the newest; events queue in order. (sheet systems: gta_bridge; docs/CONTRACT.md)
#pragma once
#include "../core/platform.hpp"
#include "../gen/protocol.hpp"
#include <deque>
#include <string>

namespace beamls
{
	class Bridge
	{
	public:
		bool start(const std::string &gtaBuild, std::uint32_t session);
		void stop(const char *reason);
		// Receive everything waiting; resend hello while not acknowledged.
		void poll(double now);

		template <typename M>
		bool send(const M &m)
		{
			if (!open_)
				return false;
			const bool ok = udp_.sendTo(port_, m.encode());
			if (ok)
				++sent_;
			return ok;
		}

		bool connected() const { return acked_ && !timedOut_; }
		bool ready() const { return connected() && ready_; }
		bool timedOut() const { return timedOut_; }
		int localPort() const { return udp_.localPort(); }
		void setPort(int port) { port_ = port; } // tests

		// Newest state from BeamNG.
		bool hasCar = false;
		msg::Car car;
		double carTime = 0.0;
		msg::Status status;
		std::string bngVersion;

		// Events from BeamNG, oldest first; the game consumes them.
		std::deque<msg::VehicleInfo> vehicleInfos;
		std::deque<msg::MenuClosed> menusClosed;
		bool byeReceived = false;

		unsigned long long sent() const { return sent_; }
		unsigned long long received() const { return received_; }
		unsigned long long malformed() const { return malformed_; }

	private:
		void handle(const char *data, int len, double now);

		platform::Udp udp_;
		bool open_ = false;
		int port_ = 0;
		bool acked_ = false;
		bool ready_ = false;
		bool timedOut_ = false;
		double lastHello_ = -1e9;
		double lastRecv_ = 0.0;
		std::string build_;
		std::uint32_t session_ = 0;
		unsigned long long sent_ = 0, received_ = 0, malformed_ = 0;
	};
}
