// Los Santos Customs: stop at a garage door in the BeamNG car and press E to repair it.
// (sheet systems: gta_repair_zones; sheet garages)
#include "game.hpp"
#include "../core/log.hpp"
#include "../gen/garages.hpp"
#include "../gen/keys.hpp"
#include "../gen/natives.hpp"
#include "../gen/settings.hpp"

namespace beamls::game::repair
{
	namespace
	{
		double g_lastRepair = -1e9;

		int repairControl()
		{
			for (const auto &k : gen::kKeys)
				if (k.action == gen::KeyAction::Repair && !k.isVk)
					return k.code;
			return 51;
		}

		const char *repairLabel()
		{
			for (const auto &k : gen::kKeys)
				if (k.action == gen::KeyAction::Repair)
					return k.label;
			return "E: repair";
		}
	}

	void update(Game &g)
	{
		if (g.mode != Mode::Drive || !g.bridge.hasCar)
			return;
		const V3 car = g.carCentre();
		if (g.bridge.car.speed > settings::repair_hold_speed)
			return;
		for (const auto &gar : gen::kGarages)
		{
			const float dx = car.x - gar.x, dy = car.y - gar.y, dz = car.z - gar.z;
			if (dx * dx + dy * dy > gar.radius * gar.radius || dz * dz > 25.0f)
				continue;
			const int control = repairControl();
			PAD::DISABLE_CONTROL_ACTION(0, control, TRUE);
			if (g.now - g_lastRepair < 3.0)
			{
				hud::help(g, std::string("Repaired at ") + gar.name);
				return;
			}
			hud::help(g, std::string(repairLabel()) + " (" + gar.name + ")");
			if (PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, control))
			{
				msg::Repair r;
				r.garage = gar.id;
				g.bridge.send(r);
				g_lastRepair = g.now;
				log::info("repair at %s", gar.id);
			}
			return;
		}
	}
}
