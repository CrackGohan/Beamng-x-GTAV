// Ground and wall probing around the BeamNG car with GTA's asynchronous shape tests, within a per-frame
// budget. A ground round is a grid of downward probes; a wall round is two rings of horizontal rays.
// (sheet systems: gta_ground_probe, gta_wall_probe; sheet obstacles)
#include "game.hpp"
#include "../gen/natives.hpp"
#include "../gen/obstacles.hpp"
#include "../gen/settings.hpp"
#include <cmath>
#include <deque>
#include <unordered_map>
#include <vector>

namespace beamls::game::probe
{
	namespace
	{
		constexpr int kFlags = 1 | 16; // map collision + objects; not vehicles or peds
		constexpr float kMissing = -10000.0f;

		struct Job
		{
			int kind = 0; // 0 ground, 1 wall
			int index = 0;
			int handle = 0;
			V3 from, to;
		};

		struct GroundRound
		{
			bool active = false;
			std::uint32_t seq = 0;
			float ox = 0, oy = 0;
			std::vector<float> z;
			int outstanding = 0;
			double started = 0.0;
		};

		struct WallRound
		{
			bool active = false;
			std::uint32_t seq = 0;
			std::vector<msg::Walls::Hit> hits;
			int outstanding = 0;
			double started = 0.0;
		};

		std::deque<Job> g_queue;   // not started yet
		std::vector<Job> g_pending; // started, waiting for a result
		GroundRound g_ground;
		WallRound g_walls;
		double g_nextGround = 0.0, g_nextWalls = 0.0;
		std::unordered_map<Hash, float> g_propRadius;

		float propRadius(Hash model)
		{
			auto it = g_propRadius.find(model);
			if (it != g_propRadius.end())
				return it->second;
			Vector3 mn, mx;
			MISC::GET_MODEL_DIMENSIONS(model, &mn, &mx);
			const float r = 0.5f * std::fmax(mx.x - mn.x, mx.y - mn.y);
			g_propRadius[model] = r;
			return r;
		}

		void startGround(Game &g)
		{
			const int n = settings::ground_n;
			const float step = settings::ground_step;
			const V3 c = g.carCentre() + g.carVel() * settings::ground_lookahead_s;
			const float half = 0.5f * step * float(n - 1);
			g_ground = GroundRound();
			g_ground.active = true;
			g_ground.seq = g.camSeq;
			// Align the grid to the step so cells keep their identity as the car moves (BeamNG reuses slabs).
			g_ground.ox = std::floor((c.x - half) / step) * step;
			g_ground.oy = std::floor((c.y - half) / step) * step;
			g_ground.z.assign(std::size_t(n * n), kMissing);
			g_ground.started = g.now;
			const float top = g.carCentre().z + settings::ground_probe_up;
			const float bottom = g.carCentre().z - settings::ground_probe_down;
			for (int j = 0; j < n; ++j)
				for (int i = 0; i < n; ++i)
				{
					Job job;
					job.kind = 0;
					job.index = j * n + i;
					const float x = g_ground.ox + float(i) * step, y = g_ground.oy + float(j) * step;
					job.from = V3(x, y, top);
					job.to = V3(x, y, bottom);
					g_queue.push_back(job);
					++g_ground.outstanding;
				}
		}

		void startWalls(Game &g)
		{
			g_walls = WallRound();
			g_walls.active = true;
			g_walls.seq = g.camSeq;
			g_walls.started = g.now;
			const V3 c = g.carCentre();
			const float groundZ = c.z - g.bridge.car.hz;
			int index = 0;
			for (float h : settings::wall_heights)
				for (int k = 0; k < settings::wall_rays; ++k)
				{
					const float a = 2.0f * 3.14159265f * float(k) / float(settings::wall_rays);
					Job job;
					job.kind = 1;
					job.index = index++;
					job.from = V3(c.x, c.y, groundZ + h);
					job.to = job.from + V3(std::cos(a), std::sin(a), 0.0f) * settings::wall_range;
					g_queue.push_back(job);
					++g_walls.outstanding;
				}
		}

		void finish(Game &g)
		{
			if (g_ground.active && (g_ground.outstanding <= 0 || g.now - g_ground.started > 1.0))
			{
				msg::Ground m;
				m.seq = g_ground.seq;
				m.ox = g_ground.ox;
				m.oy = g_ground.oy;
				m.step = settings::ground_step;
				m.n = settings::ground_n;
				m.z = g_ground.z;
				g.bridge.send(m);
				g_ground.active = false;
			}
			if (g_walls.active && (g_walls.outstanding <= 0 || g.now - g_walls.started > 1.0))
			{
				msg::Walls m;
				m.seq = g_walls.seq;
				m.hits = g_walls.hits;
				g.bridge.send(m);
				g_walls.active = false;
			}
		}

		void collect(Game &g)
		{
			for (std::size_t i = 0; i < g_pending.size();)
			{
				Job &job = g_pending[i];
				// Out-parameters are written as 8-byte script values: give each its own slot.
				alignas(8) BOOL hit[2] = {0, 0};
				alignas(8) Entity entity[2] = {0, 0};
				Vector3 end, normal;
				const int status = SHAPETEST::GET_SHAPE_TEST_RESULT(job.handle, hit, &end, &normal, entity);
				if (status == 1)
				{
					++i;
					continue; // not ready yet
				}
				if (status == 2 && hit[0])
				{
					if (job.kind == 0 && g_ground.active)
						g_ground.z[std::size_t(job.index)] = end.z;
					else if (job.kind == 1 && g_walls.active)
					{
						msg::Walls::Hit h;
						h.x = end.x;
						h.y = end.y;
						h.z = end.z;
						h.nx = normal.x;
						h.ny = normal.y;
						h.nz = normal.z;
						h.kind = gen::kObstacleWall;
						h.size = 0.0f;
						bool keep = true;
						if (entity[0] != 0)
						{
							const int type = ENTITY::GET_ENTITY_TYPE(entity[0]);
							if (type == 3) // object
							{
								const float r = propRadius(ENTITY::GET_ENTITY_MODEL(entity[0]));
								keep = 2.0f * r >= settings::prop_min_size; // thin props break off in GTA
								h.kind = gen::kObstacleProp;
								h.size = r;
							}
							else
								keep = false; // vehicles come from traffic, peds are GTA's
						}
						if (keep)
							g_walls.hits.push_back(h);
					}
				}
				if (job.kind == 0)
					--g_ground.outstanding;
				else
					--g_walls.outstanding;
				g_pending[i] = g_pending.back();
				g_pending.pop_back();
			}
		}
	}

	void reset()
	{
		g_queue.clear();
		g_pending.clear();
		g_ground = GroundRound();
		g_walls = WallRound();
	}

	void update(Game &g)
	{
		if (!g.carChosen || g.spawnPending || !g.bridge.hasCar)
			return;
		const Vector3 p = ENTITY::GET_ENTITY_COORDS(g.player, TRUE);
		const V3 d = g.carCentre() - V3(p.x, p.y, p.z);
		if (d.len() > settings::park_distance)
			return; // far from the car: it stays parked on what it has
		collect(g);
		finish(g);
		if (!g_ground.active && g.now >= g_nextGround)
		{
			g_nextGround = g.now + 1.0 / settings::ground_rate_hz;
			startGround(g);
		}
		if (!g_walls.active && g.mode == Mode::Drive && g.now >= g_nextWalls)
		{
			g_nextWalls = g.now + 1.0 / settings::wall_rate_hz;
			startWalls(g);
		}
		const Entity ignore = g.proxy.handle;
		for (int i = 0; i < settings::probes_per_tick && !g_queue.empty(); ++i)
		{
			Job job = g_queue.front();
			g_queue.pop_front();
			job.handle = SHAPETEST::START_SHAPE_TEST_LOS_PROBE(job.from.x, job.from.y, job.from.z, job.to.x, job.to.y, job.to.z,
			                                                   kFlags, ignore, 7);
			if (job.handle == 0)
			{
				if (job.kind == 0)
					--g_ground.outstanding;
				else
					--g_walls.outstanding;
				continue;
			}
			g_pending.push_back(job);
		}
	}
}
