// GTA's clock to BeamNG every few seconds, so the sun on the BeamNG car matches. (sheet systems: gta_time_sync)
#include "game.hpp"
#include "../gen/natives.hpp"
#include "../gen/settings.hpp"

namespace beamls::game::timesync
{
	namespace
	{
		double g_next = 0.0;
	}

	void update(Game &g)
	{
		if (g.now < g_next)
			return;
		g_next = g.now + settings::time_rate_s;
		msg::Time t;
		t.hour = CLOCK::GET_CLOCK_HOURS();
		t.minute = CLOCK::GET_CLOCK_MINUTES();
		g.bridge.send(t);
	}
}
