// GTA vehicles near the BeamNG car (traffic and police) with their box centre, size, pose and velocity,
// for BeamNG's solid proxies. (sheet systems: gta_traffic)
#include "game.hpp"
#include "../gen/natives.hpp"
#include "../gen/settings.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace beamls::game::traffic
{
	namespace
	{
		double g_next = 0.0;
		struct Dims
		{
			V3 centre, size;
		};
		std::unordered_map<Hash, Dims> g_dims;

		const Dims &dims(Hash model)
		{
			auto it = g_dims.find(model);
			if (it != g_dims.end())
				return it->second;
			Vector3 mn, mx;
			MISC::GET_MODEL_DIMENSIONS(model, &mn, &mx);
			Dims d;
			d.centre = V3((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
			d.size = V3(mx.x - mn.x, mx.y - mn.y, mx.z - mn.z);
			return g_dims[model] = d;
		}
	}

	void update(Game &g)
	{
		if (!g.carChosen || g.spawnPending || !g.bridge.hasCar || g.now < g_next)
			return;
		g_next = g.now + 1.0 / settings::traffic_rate_hz;
		// The native fills an array of script values: [0] = capacity, then one handle per slot.
		constexpr int kCap = 32;
		alignas(8) std::int64_t arr[kCap + 1] = {};
		arr[0] = kCap;
		const int n = PED::GET_PED_NEARBY_VEHICLES(g.player, reinterpret_cast<Any *>(arr));
		const V3 car = g.carCentre();
		std::vector<std::pair<float, msg::Traffic::Car>> found;
		for (int i = 1; i <= n && i <= kCap; ++i)
		{
			const Vehicle v = static_cast<Vehicle>(arr[i]);
			if (v == 0 || v == g.proxy.handle || !ENTITY::DOES_ENTITY_EXIST(v))
				continue;
			const Vector3 p = ENTITY::GET_ENTITY_COORDS(v, TRUE);
			const V3 pos(p.x, p.y, p.z);
			const float dist = (pos - car).len();
			if (dist > settings::traffic_range)
				continue;
			alignas(8) float q[4][2] = {};
			ENTITY::GET_ENTITY_QUATERNION(v, q[0], q[1], q[2], q[3]);
			const Quat rot = Quat(q[0][0], q[1][0], q[2][0], q[3][0]).norm();
			const Vector3 vel = ENTITY::GET_ENTITY_VELOCITY(v);
			const Dims &d = dims(ENTITY::GET_ENTITY_MODEL(v));
			const V3 centre = pos + rot.rotate(d.centre);
			msg::Traffic::Car c;
			c.id = v;
			c.cls = VEHICLE::GET_VEHICLE_CLASS(v);
			c.x = centre.x;
			c.y = centre.y;
			c.z = centre.z;
			c.qx = rot.x;
			c.qy = rot.y;
			c.qz = rot.z;
			c.qw = rot.w;
			c.vx = vel.x;
			c.vy = vel.y;
			c.vz = vel.z;
			c.l = d.size.y;
			c.w = d.size.x;
			c.h = d.size.z;
			found.emplace_back(dist, c);
		}
		std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
		msg::Traffic m;
		m.seq = g.camSeq;
		for (std::size_t i = 0; i < found.size() && int(i) < settings::traffic_max; ++i)
			m.cars.push_back(found[i].second);
		g.bridge.send(m);
	}
}
