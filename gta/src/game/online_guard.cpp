// Story mode only: the moment GTA Online starts (or is loading) BeamLS deletes what it made and stays off
// until the game restarts. (sheet systems: gta_online_guard)
#include "game.hpp"
#include "../core/log.hpp"
#include "../gen/natives.hpp"
#include "../gen/settings.hpp"

namespace beamls::game::online
{
	namespace
	{
		constexpr Hash joaat(const char *s)
		{
			Hash h = 0;
			for (; *s; ++s)
			{
				char c = *s;
				if (c >= 'A' && c <= 'Z')
					c = char(c - 'A' + 'a');
				h += static_cast<unsigned char>(c);
				h += h << 10;
				h ^= h >> 6;
			}
			h += h << 3;
			h ^= h >> 11;
			h += h << 15;
			return h;
		}
		static_assert(joaat("adder") == 0xB779A091u, "joaat");
	}

	bool check(Game &)
	{
		if (NETWORK::NETWORK_IS_SESSION_STARTED() || NETWORK::NETWORK_IS_GAME_IN_PROGRESS())
		{
			log::warn("GTA Online session detected");
			return true;
		}
		for (const char *name : settings::online_scripts)
		{
			if (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(joaat(name)) > 0)
			{
				log::warn("GTA Online script %s is running", name);
				return true;
			}
		}
		return false;
	}
}
