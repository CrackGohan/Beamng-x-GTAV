// What the compositor needs from the script side, handed over under a lock: GTA's recent cameras, the
// BeamNG car's recent poses with the camera BeamNG drew them from, and whether to draw at all.
// (sheet systems: gta_composite_state)
#pragma once
#include "../core/mathx.hpp"
#include <cstdint>

namespace beamls::composite
{
	struct Camera
	{
		std::uint32_t seq = 0;
		V3 pos;
		Quat rot;
		float fov = 50.0f, nearClip = 0.15f;
		bool valid = false;
	};

	struct CarPose
	{
		std::uint32_t seq = 0, camSeq = 0;
		V3 centre;   // box centre
		Quat rot;    // GTA convention
		V3 half;     // half extents: x right, y forward, z up
		V3 vel;
		bool valid = false;
	};

	constexpr int kHistory = 16;

	struct State
	{
		bool draw = false;            // a car exists and the player is in a state where it is shown
		double tickTime = 0.0;        // platform::now() of the last script tick
		Camera cams[kHistory];        // newest at (camHead - 1) % kHistory
		unsigned camHead = 0;
		CarPose cars[kHistory];
		unsigned carHead = 0;
		void *beamngWindow = nullptr;
	};

	// Script side writes, render side reads; both copy the whole struct under the lock.
	void write(const State &s);
	State read();
	// Camera BeamNG used for camSeq, if still in the history.
	bool findCamera(const State &s, std::uint32_t camSeq, Camera &out);
	const CarPose *newestCar(const State &s);
	const Camera *newestCamera(const State &s);
}
