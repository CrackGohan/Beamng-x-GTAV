// Runs BeamLS's tick once per game frame on GTA's script thread, inside a running game script's context,
// by detouring the handler of a native every story-mode script calls each frame (settings.tick_native).
// Natives that need a script thread (CREATE_VEHICLE, ...) then work. (sheet systems: gta_script_tick)
#pragma once

namespace beamls::scripthook
{
	using TickFn = void (*)();
	bool install(TickFn tick); // after invoker::init succeeded
	void remove();
	bool installed();
	unsigned long long ticks();
	double lastTickTime(); // platform::now() of the last tick; the compositor hides the car when it is stale
}
