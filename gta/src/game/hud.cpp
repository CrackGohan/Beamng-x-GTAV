// Help text and notifications in GTA's own HUD. (sheet systems: gta_hud)
#include "game.hpp"
#include "../core/log.hpp"
#include "../gen/natives.hpp"

namespace beamls::game::hud
{
	void help(Game &g, const std::string &text) { g.help = text; }

	void notify(Game &g, const std::string &text)
	{
		log::info("notice: %s", text.c_str());
		if (g.notices.size() < 8)
			g.notices.push_back(text);
		if (g.online)
			draw(g); // shutting down: no later frame will show it
	}

	void draw(Game &g)
	{
		if (!g.notices.empty() && g.now - g.lastNotice >= 0.5)
		{
			g.lastNotice = g.now;
			HUD::BEGIN_TEXT_COMMAND_THEFEED_POST("STRING");
			HUD::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME(g.notices.front().c_str());
			HUD::END_TEXT_COMMAND_THEFEED_POST_TICKER(FALSE, TRUE);
			g.notices.pop_front();
		}
		if (g.help.empty())
			return;
		HUD::BEGIN_TEXT_COMMAND_DISPLAY_HELP("STRING");
		HUD::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME(g.help.c_str());
		HUD::END_TEXT_COMMAND_DISPLAY_HELP(0, FALSE, FALSE, -1);
	}
}
