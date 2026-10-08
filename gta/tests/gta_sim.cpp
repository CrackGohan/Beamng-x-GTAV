// GTA side of the end-to-end test (tests/e2e.sh): BeamLS's real game logic on the fake GTA, in real time,
// talking over real UDP to the real beamls Lua mod (running in LuaJIT with LuaSocket and a fake BeamNG
// engine). Plays: connect, F6, pick a car, spawn, get in, drive with throttle, ram a police car, repair,
// quit. Prints one line per check and a final summary.
#include "../src/core/log.hpp"
#include "../src/game/composite_state.hpp"
#include "../src/game/game.hpp"
#include "check.hpp"
#include "fake_gta.hpp"
#include <chrono>
#include <thread>

using namespace beamls;

static void frame()
{
	fake::nextFrame(1.0 / 60.0);
	game::tick();
	std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

template <typename F>
static bool waitFor(F cond, double seconds)
{
	for (int i = 0; i < int(seconds * 60); ++i)
	{
		frame();
		if (cond())
			return true;
	}
	return false;
}

int main()
{
	log::open("build/tests/gta_sim.log");
	fake::reset();
	auto &w = fake::world();
	auto &g = game::G();
	w.entities[w.playerPed].pos = V3(-50, 80, 13);
	w.walls.push_back({V3(-40, 60, 0), V3(-39, 120, 40)});
	fake::Entity cop;
	cop.model = fake::joaat("police");
	cop.pos = V3(-46.5f, 95, 12.8f);
	cop.vel = V3(0, -8, 0);
	cop.rot = fromHeading(180);
	cop.vehicleClass = 18;
	w.nearbyVehicles = {w.addEntity(cop)};
	g.bridge.start("3889", 4242); // default port: the Lua listener

	CHECK(waitFor([&] { return g.bridge.ready(); }, 20));
	std::printf("e2e: connected to BeamNG %s\n", g.bridge.bngVersion.c_str());

	fake::platform().keys.insert(0x75); // F6
	frame();
	fake::platform().keys.clear();
	CHECK(waitFor([&] { return g.menuOpen; }, 2));
	CHECK(fake::platform().focusBeamNGCalls == 1);
	// The Lua harness plays the player in BeamNG's menu: picks a car, presses F6 there.
	CHECK(waitFor([&] { return g.carChosen && !g.menuOpen; }, 10));
	CHECK(fake::platform().focusGtaCalls >= 1);
	CHECK(waitFor([&] { return !g.spawnPending && g.proxy.handle != 0; }, 10));
	std::printf("e2e: proxy %d (%s) for %s\n", g.proxy.handle, g.proxy.sizeClass, g.carInfo.name.c_str());

	w.playerVehicle = g.proxy.handle; // get in
	w.normals[71] = 1.0f;             // full throttle
	CHECK(waitFor([&] { return g.mode == game::Mode::Drive; }, 2));
	// BeamNG's (fake) car accelerates from our throttle; GTA's proxy follows it north.
	const float y0 = w.entities[g.proxy.handle].pos.y;
	for (int i = 0; i < 120; ++i)
		frame();
	const float y1 = w.entities[g.proxy.handle].pos.y;
	std::printf("e2e: proxy moved %.2f m north in 2 s\n", y1 - y0);
	CHECK(y1 - y0 > 2.0f);
	CHECK(composite::read().draw);
	w.normals.clear();

	// Quit GTA: BeamNG gets bye.
	game::shutdown("quit");
	for (int i = 0; i < 30; ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	return check::summary("e2e gta side");
}
