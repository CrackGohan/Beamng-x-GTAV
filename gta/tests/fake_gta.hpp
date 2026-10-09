// A small stand-in for GTA V's natives so BeamLS's game logic runs on Linux: one fake handler per row of
// sheets/natives.json, reading arguments from and writing results to a NativeCtx exactly like the game's
// handlers (including vector out-parameters through the context's buffers). It is a model, not GTA:
// passing tests here says the logic is consistent, not that GTA behaves like this.
#pragma once
#include "../src/core/invoker.hpp"
#include "../src/core/mathx.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fake
{
	struct Platform
	{
		double time = 1.0;
		std::set<int> keys;
		bool gtaFocused = true;
		std::string exeVersion = "1.0.3889.0";
		void *beamngWindow = reinterpret_cast<void *>(0x1234);
		int focusBeamNGCalls = 0, focusGtaCalls = 0;
	};
	Platform &platform();

	struct Entity
	{
		int type = 2; // 1 ped, 2 vehicle, 3 object
		std::uint32_t model = 0;
		beamls::V3 pos, vel, angVel;
		beamls::Quat rot;
		bool visible = true, collision = true, mission = false, invincible = false, exists = true;
		bool inWater = false;
		int vehicleClass = 1;
		std::string audio;
		int doorLock = 0;
		int fixedCount = 0;
		int setCoordsCount = 0;
	};

	struct Box
	{
		beamls::V3 min, max;
	};

	struct World
	{
		int frame = 0;
		// player
		int playerPed = 1;
		int playerVehicle = 0; // vehicle the player sits in
		bool playerDead = false;
		// camera
		beamls::V3 camPos{0, -6, 12};
		beamls::V3 camRot{-10, 0, 0}; // pitch, roll, yaw (degrees)
		float camFov = 50.0f, camNear = 0.15f;
		// entities
		std::map<int, Entity> entities;
		int nextHandle = 100;
		std::map<std::uint32_t, std::pair<beamls::V3, beamls::V3>> modelDims; // min, max
		std::set<std::uint32_t> requested, loaded;
		// world geometry
		float groundBase = 10.0f, groundSlopeX = 0.05f;
		std::vector<Box> walls;
		std::vector<int> nearbyVehicles;
		// controls
		std::map<int, float> normals;
		std::set<int> pressed, justPressed, disabled;
		// state flags
		bool loading = false, cutscene = false, switching = false, faded = false, pauseMenu = false;
		bool sessionStarted = false, gameInProgress = false;
		std::map<std::uint32_t, int> scripts;
		int hours = 14, minutes = 30;
		// HUD
		std::vector<std::string> helps, feed;
		std::string pending;
		// shape tests
		struct Probe
		{
			beamls::V3 from, to;
			int ignore = 0, flags = 0, frame = 0;
		};
		std::map<int, Probe> probes;
		int nextProbe = 1;
		int probesStarted = 0;
		// counts
		std::map<std::string, int> calls;

		float groundAt(float x, float /*y*/) const { return groundBase + groundSlopeX * x; }
		int addEntity(const Entity &e)
		{
			const int h = nextHandle++;
			entities[h] = e;
			return h;
		}
	};
	World &world();
	void reset();
	void nextFrame(double dt = 1.0 / 60.0); // time and frame advance; justPressed cleared

	std::uint32_t joaat(const char *s);
}
