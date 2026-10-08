// The GTA side of a mashup session: shared state and the per-system update functions, all run once per
// game frame from the script tick (main.cpp). One namespace per row of sheet systems.
#pragma once
#include "../core/mathx.hpp"
#include "../core/types.hpp"
#include "../net/bridge.hpp"
#include <deque>
#include <string>

namespace beamls::game
{
	enum class Mode { WaitBeamNG, OnFoot, Drive, MenuVehicle, MenuTuning, Paused, Off };
	const char *wireName(Mode m); // the mode message's state value

	struct CamSample
	{
		std::uint32_t seq = 0;
		V3 pos;
		Quat rot;
		float fov = 50.0f, nearClip = 0.15f;
		int w = 1920, h = 1080;
		double time = 0.0;
	};

	struct Proxy
	{
		Vehicle handle = 0;
		Hash model = 0;
		const char *sizeClass = "";
		V3 modelCenter;      // centre of the model's box in model space
		V3 modelSize;
		double modelRequested = 0.0;
		double lastRefresh = 0.0;
		bool warpPlayerIn = false;
	};

	struct Game
	{
		Bridge bridge;
		double now = 0.0, dt = 0.0;
		std::string build;

		Mode mode = Mode::WaitBeamNG;
		Mode sentMode = Mode::Off;
		double lastModeSent = -1e9;
		bool userOff = false;    // F8
		bool online = false;     // GTA Online seen: off for good
		bool gated = false;      // loading, cutscene, switch, death, pause menu

		Ped player = 0;
		bool playerInProxy = false;

		bool carChosen = false;  // the player picked a BeamNG car this session
		bool spawnPending = false;
		V3 spawnPos;
		float spawnHeading = 0.0f;
		double spawnAsked = -1e9, spawnStarted = 0.0;

		bool haveCarInfo = false;
		msg::VehicleInfo carInfo;
		Proxy proxy;

		bool menuOpen = false;
		Mode menuMode = Mode::MenuVehicle;
		double menuOpenedAt = 0.0;

		std::uint32_t camSeq = 0;
		CamSample cam;
		std::string help;        // help text to show this frame
		std::deque<std::string> notices; // feed posts, shown one every half second
		double lastNotice = -1e9;

		// Car pose (GTA world, GTA convention) extrapolated to now; valid when bridge.hasCar.
		V3 carCentre() const;
		Quat carRot() const;
		V3 carVel() const;
	};

	Game &G();
	void tick();                        // once per game frame (scripthook)
	void shutdown(const char *reason);  // delete what we made in GTA and tell BeamNG
	void setMode(Game &g, Mode m, const char *reason);

	namespace online { bool check(Game &g); }
	namespace gate { bool check(Game &g); }
	namespace camera { void update(Game &g); }
	namespace input { void update(Game &g); }
	namespace proxy
	{
		void update(Game &g);
		void destroy(Game &g);
		void requestSpawnNextToPlayer(Game &g);
		void onVehicleInfo(Game &g, const msg::VehicleInfo &info);
		bool exists(Game &g);
	}
	namespace probe
	{
		void update(Game &g);
		void reset();
	}
	namespace traffic { void update(Game &g); }
	namespace hotkeys
	{
		void update(Game &g);
		void onMenuClosed(Game &g, const msg::MenuClosed &m);
	}
	namespace repair { void update(Game &g); }
	namespace timesync { void update(Game &g); }
	namespace hud
	{
		void help(Game &g, const std::string &text);
		void notify(Game &g, const std::string &text);
		void draw(Game &g);
	}
	namespace composite { void publish(Game &g); }
}
