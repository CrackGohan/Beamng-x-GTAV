// Pause the mashup during loading screens, cutscenes, character switches, death, fades and the pause menu.
// (sheet systems: gta_state_gate)
#include "game.hpp"
#include "../gen/natives.hpp"

namespace beamls::game::gate
{
	bool check(Game &)
	{
		return DLC::GET_IS_LOADING_SCREEN_ACTIVE() || CUTSCENE::IS_CUTSCENE_ACTIVE() ||
		       STREAMING::IS_PLAYER_SWITCH_IN_PROGRESS() || CAMERA::IS_SCREEN_FADED_OUT() ||
		       PLAYER::IS_PLAYER_DEAD(PLAYER::PLAYER_ID()) || HUD::IS_PAUSE_MENU_ACTIVE();
	}
}
