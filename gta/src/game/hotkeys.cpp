// F6 vehicle selector, F7 tuning, F8 on/off; handing focus to BeamNG for its menus and back.
// (sheet systems: gta_hotkeys, gta_focus; sheet keys)
#include "game.hpp"
#include "../core/log.hpp"
#include "../core/platform.hpp"
#include "../gen/keys.hpp"
#include "../gen/natives.hpp"

namespace beamls::game::hotkeys
{
	namespace
	{
		bool g_wasDown[8] = {};

		bool pressed(int index, int vk)
		{
			const bool down = platform::keyDown(vk);
			const bool edge = down && !g_wasDown[index];
			g_wasDown[index] = down;
			return edge;
		}

		void openMenu(Game &g, Mode which)
		{
			if (!g.bridge.ready())
			{
				hud::notify(g, "BeamNG isn't running yet. Melty starts it with the mashup; give it a moment.");
				return;
			}
			if (which == Mode::MenuTuning && !g.carChosen)
			{
				hud::notify(g, "Choose a BeamNG car first (F6).");
				return;
			}
			g.menuOpen = true;
			g.menuMode = which;
			g.menuOpenedAt = g.now;
			setMode(g, which, which == Mode::MenuTuning ? "F7" : "F6");
			if (!platform::focusBeamNG())
				hud::notify(g, "Switch to the BeamNG window (Alt+Tab) to use its menu, then press F6 there to come back.");
			log::info("menu opened: %s", wireName(which));
		}

		void closeFromGta(Game &g)
		{
			g.menuOpen = false;
			log::info("menu closed from GTA");
		}
	}

	void onMenuClosed(Game &g, const msg::MenuClosed &m)
	{
		if (g.menuOpen)
		{
			g.menuOpen = false;
			platform::focusGta();
		}
		log::info("BeamNG menu closed (car changed: %d)", int(m.changed));
		if (!g.carChosen)
		{
			g.carChosen = true;
			proxy::requestSpawnNextToPlayer(g);
		}
	}

	void update(Game &g)
	{
		if (!platform::gameHasFocus())
		{
			for (bool &w : g_wasDown)
				w = false;
			return;
		}
		int index = 0;
		for (const auto &k : gen::kKeys)
		{
			const int i = index++;
			if (!k.isVk)
				continue; // control-based keys (repair) are read where they apply
			if (!pressed(i, k.code))
				continue;
			if (g.gated && k.action != gen::KeyAction::ToggleMashup)
				continue;
			switch (k.action)
			{
			case gen::KeyAction::OpenVehicles:
				if (g.menuOpen)
					closeFromGta(g);
				else if (!g.userOff)
					openMenu(g, Mode::MenuVehicle);
				break;
			case gen::KeyAction::OpenTuning:
				if (g.menuOpen)
					closeFromGta(g);
				else if (!g.userOff)
					openMenu(g, Mode::MenuTuning);
				break;
			case gen::KeyAction::ToggleMashup:
				g.userOff = !g.userOff;
				log::info("mashup switched %s with F8", g.userOff ? "off" : "on");
				if (g.userOff)
				{
					g.menuOpen = false;
					proxy::destroy(g);
					probe::reset();
					hud::notify(g, "BeamNG x Los Santos off. F8 brings your car back.");
				}
				else
					hud::notify(g, "BeamNG x Los Santos on.");
				break;
			default:
				break;
			}
		}
	}
}
