// The mashup's state machine on the GTA side, run once per game frame. (sheet systems: gta_main)
#include "game.hpp"
#include "../core/log.hpp"
#include "../core/platform.hpp"
#include "../gen/natives.hpp"
#include "../gen/settings.hpp"

namespace beamls::game
{
	Game &G()
	{
		static Game g;
		return g;
	}

	const char *wireName(Mode m)
	{
		switch (m)
		{
		case Mode::OnFoot: return "onfoot";
		case Mode::Drive: return "drive";
		case Mode::MenuVehicle: return "menu_vehicle";
		case Mode::MenuTuning: return "menu_tuning";
		case Mode::Paused: return "paused";
		case Mode::Off: return "off";
		case Mode::WaitBeamNG: return "onfoot";
		}
		return "off";
	}

	V3 Game::carCentre() const
	{
		const auto &c = bridge.car;
		const float age = static_cast<float>(now - bridge.carTime);
		const float a = age < 0.25f ? age : 0.25f; // never extrapolate further than a quarter second
		return V3(c.bx + c.vx * a, c.by + c.vy * a, c.bz + c.vz * a);
	}

	Quat Game::carRot() const
	{
		const auto &c = bridge.car;
		return Quat(c.qx, c.qy, c.qz, c.qw).norm();
	}

	V3 Game::carVel() const
	{
		const auto &c = bridge.car;
		return V3(c.vx, c.vy, c.vz);
	}

	void setMode(Game &g, Mode m, const char *reason)
	{
		if (g.mode != m)
			log::info("mode %s -> %s (%s)", wireName(g.mode), wireName(m), reason);
		g.mode = m;
		if (!g.bridge.connected())
			return;
		if (m != g.sentMode || g.now - g.lastModeSent >= 1.0)
		{
			msg::Mode mm;
			mm.state = wireName(m);
			mm.reason = reason;
			if (g.bridge.send(mm))
			{
				g.sentMode = m;
				g.lastModeSent = g.now;
			}
		}
	}

	void shutdown(const char *reason)
	{
		Game &g = G();
		log::info("shutting down: %s", reason);
		proxy::destroy(g);
		probe::reset();
		g.menuOpen = false;
		g.carChosen = false;
		g.mode = Mode::Off;
		g.bridge.stop(reason);
		composite::publish(g);
	}

	void tick()
	{
		Game &g = G();
		const double t = platform::now();
		g.dt = g.now > 0.0 ? t - g.now : 0.0;
		g.now = t;
		g.help.clear();
		if (g.online)
			return;
		if (online::check(g))
		{
			g.online = true;
			g.lastNotice = -1e9; // show it now: no later frame will
			hud::notify(g, "BeamNG x Los Santos is story mode only and has switched itself off.");
			shutdown("online");
			return;
		}

		g.player = PLAYER::PLAYER_PED_ID();
		g.bridge.poll(g.now);
		if (g.bridge.byeReceived || g.bridge.timedOut())
		{
			if (g.carChosen)
			{
				log::info("BeamNG left: the BeamNG car is gone");
				proxy::destroy(g);
				g.carChosen = false;
				g.spawnPending = false;
				g.menuOpen = false;
				g.haveCarInfo = false;
				hud::notify(g, "BeamNG closed. Start it again from Melty to bring your car back.");
			}
			g.bridge.byeReceived = false;
		}
		while (!g.bridge.vehicleInfos.empty())
		{
			proxy::onVehicleInfo(g, g.bridge.vehicleInfos.front());
			g.bridge.vehicleInfos.pop_front();
		}
		while (!g.bridge.menusClosed.empty())
		{
			hotkeys::onMenuClosed(g, g.bridge.menusClosed.front());
			g.bridge.menusClosed.pop_front();
		}

		g.gated = gate::check(g);
		hotkeys::update(g);

		if (g.userOff)
		{
			setMode(g, Mode::Off, "switched off with F8");
			composite::publish(g);
			hud::draw(g);
			return;
		}
		if (!g.bridge.connected())
		{
			g.mode = Mode::WaitBeamNG;
			composite::publish(g);
			hud::draw(g);
			return;
		}

		g.playerInProxy = proxy::exists(g) && PED::IS_PED_IN_VEHICLE(g.player, g.proxy.handle, FALSE);
		if (g.gated)
			setMode(g, Mode::Paused, "loading, cutscene, switch, death or pause menu");
		else if (g.menuOpen)
			setMode(g, g.menuMode, "BeamNG menu open");
		else if (g.playerInProxy)
			setMode(g, Mode::Drive, "player in the BeamNG car");
		else
			setMode(g, Mode::OnFoot, "player on foot");

		if (!g.gated)
		{
			camera::update(g);
			proxy::update(g);
			if (g.mode == Mode::Drive)
				input::update(g);
			probe::update(g);
			traffic::update(g);
			repair::update(g);
			timesync::update(g);
			if (!g.carChosen && g.bridge.ready() && !g.menuOpen)
				hud::help(g, "F6: choose a BeamNG car");
		}
		composite::publish(g);
		hud::draw(g);
	}
}
