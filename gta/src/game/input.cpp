// While the player drives the proxy: GTA's own vehicle controls are disabled and their values (keyboard or
// pad, GTA merges both) go to BeamNG. (sheet systems: gta_input; sheet controls)
#include "game.hpp"
#include "../gen/controls.hpp"
#include "../gen/natives.hpp"

namespace beamls::game::input
{
	void update(Game &g)
	{
		msg::Input m;
		m.seq = g.camSeq;
		for (const auto &c : gen::kControls)
		{
			PAD::DISABLE_CONTROL_ACTION(0, c.control, TRUE);
			float v = 0.0f;
			switch (c.kind)
			{
			case gen::ControlKind::Axis: v = PAD::GET_DISABLED_CONTROL_NORMAL(0, c.control); break;
			case gen::ControlKind::Button: v = PAD::IS_DISABLED_CONTROL_PRESSED(0, c.control) ? 1.0f : 0.0f; break;
			case gen::ControlKind::Press: v = PAD::IS_DISABLED_CONTROL_JUST_PRESSED(0, c.control) ? 1.0f : 0.0f; break;
			}
			v *= c.scale;
			switch (c.field)
			{
			case gen::InputField::Throttle: m.throttle = clampf(v, 0.0f, 1.0f); break;
			case gen::InputField::Brake: m.brake = clampf(v, 0.0f, 1.0f); break;
			case gen::InputField::Steer: m.steer = clampf(v, -1.0f, 1.0f); break;
			case gen::InputField::Handbrake: m.handbrake = clampf(v, 0.0f, 1.0f); break;
			case gen::InputField::Horn: m.horn = v > 0.5f; break;
			case gen::InputField::Lights: m.lights = v > 0.5f; break;
			case gen::InputField::None: break;
			}
		}
		g.bridge.send(m);
	}
}
