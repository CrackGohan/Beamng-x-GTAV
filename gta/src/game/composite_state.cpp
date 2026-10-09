#include "composite_state.hpp"
#include "game.hpp"
#include "../core/platform.hpp"
#include <mutex>

namespace beamls::composite
{
	namespace
	{
		std::mutex g_lock;
		State g_state;
	}

	void write(const State &s)
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_state = s;
	}

	State read()
	{
		std::lock_guard<std::mutex> lock(g_lock);
		return g_state;
	}

	bool findCamera(const State &s, std::uint32_t camSeq, Camera &out)
	{
		for (const auto &c : s.cams)
			if (c.valid && c.seq == camSeq)
			{
				out = c;
				return true;
			}
		return false;
	}

	const CarPose *newestCar(const State &s)
	{
		const CarPose &c = s.cars[(s.carHead + kHistory - 1) % kHistory];
		return c.valid ? &c : nullptr;
	}

	const Camera *newestCamera(const State &s)
	{
		const Camera &c = s.cams[(s.camHead + kHistory - 1) % kHistory];
		return c.valid ? &c : nullptr;
	}
}

namespace beamls::game::composite
{
	// Called at the end of every tick: append this frame's camera and the newest car pose.
	void publish(Game &g)
	{
		static beamls::composite::State s;
		static std::uint32_t lastCarSeq = 0;
		s.tickTime = g.now;
		s.beamngWindow = platform::beamngWindow();
		const bool showMode = g.mode == Mode::Drive || g.mode == Mode::OnFoot;
		s.draw = showMode && g.carChosen && !g.spawnPending && g.bridge.hasCar && !g.gated && !g.userOff && !g.online;

		auto &cam = s.cams[s.camHead % beamls::composite::kHistory];
		cam.seq = g.cam.seq;
		cam.pos = g.cam.pos;
		cam.rot = g.cam.rot;
		cam.fov = g.cam.fov;
		cam.nearClip = g.cam.nearClip;
		cam.valid = g.cam.seq != 0;
		++s.camHead;

		if (g.bridge.hasCar && g.bridge.car.seq != lastCarSeq)
		{
			lastCarSeq = g.bridge.car.seq;
			const auto &c = g.bridge.car;
			auto &p = s.cars[s.carHead % beamls::composite::kHistory];
			p.seq = c.seq;
			p.camSeq = c.cam_seq;
			p.centre = V3(c.bx, c.by, c.bz);
			p.rot = Quat(c.qx, c.qy, c.qz, c.qw).norm();
			p.half = V3(c.hx, c.hy, c.hz);
			p.vel = V3(c.vx, c.vy, c.vz);
			p.valid = true;
			++s.carHead;
		}
		beamls::composite::write(s);
	}
}
