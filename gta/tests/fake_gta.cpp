#include "fake_gta.hpp"
#include "../src/gen/xmap.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>

using beamls::NativeCtx;
using beamls::Quat;
using beamls::V3;

namespace fake
{
	Platform &platform()
	{
		static Platform p;
		return p;
	}
	World &world()
	{
		static World w;
		return w;
	}

	std::uint32_t joaat(const char *s)
	{
		std::uint32_t h = 0;
		for (; *s; ++s)
		{
			char ch = *s;
			if (ch >= 'A' && ch <= 'Z')
				ch = char(ch - 'A' + 'a');
			h += static_cast<unsigned char>(ch);
			h += h << 10;
			h ^= h >> 6;
		}
		h += h << 3;
		h ^= h >> 11;
		h += h << 15;
		return h;
	}

	void reset()
	{
		world() = World();
		platform() = Platform();
		World &w = world();
		Entity ped;
		ped.type = 1;
		ped.model = joaat("player_zero");
		ped.pos = V3(0, 0, 11);
		w.entities[w.playerPed] = ped;
		auto dims = [&](const char *m, V3 mn, V3 mx) { w.modelDims[joaat(m)] = {mn, mx}; };
		dims("issi2", {-0.8f, -1.6f, -0.5f}, {0.8f, 1.6f, 1.0f});
		dims("comet2", {-0.95f, -2.2f, -0.5f}, {0.95f, 2.2f, 0.8f});
		dims("blista", {-0.9f, -2.0f, -0.5f}, {0.9f, 2.0f, 1.0f});
		dims("fugitive", {-0.95f, -2.45f, -0.6f}, {0.95f, 2.45f, 0.9f});
		dims("baller", {-1.0f, -2.45f, -0.6f}, {1.0f, 2.45f, 1.3f});
		dims("bison", {-1.0f, -2.8f, -0.6f}, {1.0f, 2.8f, 1.3f});
		dims("speedo", {-1.0f, -2.65f, -0.6f}, {1.0f, 2.65f, 1.8f});
		dims("mule", {-1.2f, -3.75f, -0.8f}, {1.2f, 3.75f, 2.5f});
		dims("bus", {-1.3f, -6.25f, -0.8f}, {1.3f, 6.25f, 3.0f});
		dims("police", {-0.95f, -2.5f, -0.6f}, {0.95f, 2.5f, 1.0f});
		dims("prop_bollard_02a", {-0.15f, -0.15f, 0.0f}, {0.15f, 0.15f, 1.0f});
		dims("prop_dumpster_01a", {-1.0f, -0.6f, 0.0f}, {1.0f, 0.6f, 1.5f});
	}

	void nextFrame(double dt)
	{
		++world().frame;
		platform().time += dt;
		world().justPressed.clear();
		world().disabled.clear();
	}
}

namespace
{
	using fake::world;
	template <typename T>
	T A(NativeCtx *c, int i)
	{
		T v;
		std::memcpy(&v, static_cast<std::uint64_t *>(c->args) + i, sizeof(T));
		return v;
	}
	template <typename T>
	void R(NativeCtx *c, T v) { std::memcpy(c->ret, &v, sizeof(T)); }
	void RV(NativeCtx *c, const V3 &v)
	{
		const Vector3 out = v3(v.x, v.y, v.z);
		std::memcpy(c->ret, &out, sizeof(out));
	}
	// A Vector3* out-parameter, written the way the game does: into the context's buffer, copied out later.
	void outVec(NativeCtx *c, int arg, const V3 &v)
	{
		float *dst = A<float *>(c, arg);
		const int k = c->bufferCount++;
		c->orig[k] = dst;
		c->buffers[k][0] = v.x;
		c->buffers[k][1] = v.y;
		c->buffers[k][2] = v.z;
		c->buffers[k][3] = 0.0f;
	}
	fake::Entity *ent(int h)
	{
		auto it = world().entities.find(h);
		return it == world().entities.end() || !it->second.exists ? nullptr : &it->second;
	}
	void count(const char *n) { ++world().calls[n]; }

	// Segment against an axis-aligned box: entry fraction and normal.
	bool segBox(const V3 &a, const V3 &b, const V3 &mn, const V3 &mx, float &t, V3 &n)
	{
		float t0 = 0.0f, t1 = 1.0f;
		V3 nn;
		const float A3[3] = {a.x, a.y, a.z}, D3[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
		const float MN[3] = {mn.x, mn.y, mn.z}, MX[3] = {mx.x, mx.y, mx.z};
		for (int i = 0; i < 3; ++i)
		{
			if (std::fabs(D3[i]) < 1e-9f)
			{
				if (A3[i] < MN[i] || A3[i] > MX[i])
					return false;
				continue;
			}
			float ta = (MN[i] - A3[i]) / D3[i], tb = (MX[i] - A3[i]) / D3[i];
			float sign = -1.0f;
			if (ta > tb)
			{
				std::swap(ta, tb);
				sign = 1.0f;
			}
			if (ta > t0)
			{
				t0 = ta;
				nn = V3(i == 0 ? sign : 0, i == 1 ? sign : 0, i == 2 ? sign : 0);
			}
			t1 = std::fmin(t1, tb);
			if (t0 > t1)
				return false;
		}
		t = t0;
		n = nn;
		return true;
	}

#define H(name) void h_##name(NativeCtx *c)
	// HUD
	H(BEGIN_TEXT_COMMAND_DISPLAY_HELP) { count("help"); world().pending.clear(); (void)c; }
	H(BEGIN_TEXT_COMMAND_THEFEED_POST) { world().pending.clear(); (void)c; }
	H(ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME) { world().pending += A<const char *>(c, 0); }
	H(END_TEXT_COMMAND_DISPLAY_HELP) { world().helps.push_back(world().pending); (void)c; }
	H(END_TEXT_COMMAND_THEFEED_POST_TICKER) { world().feed.push_back(world().pending); R<int>(c, 1); }
	// vehicles and entities
	H(CREATE_VEHICLE)
	{
		count("CREATE_VEHICLE");
		const std::uint32_t model = A<std::uint32_t>(c, 0);
		if (!world().loaded.count(model))
		{
			R<int>(c, 0);
			return;
		}
		fake::Entity e;
		e.type = 2;
		e.model = model;
		e.pos = V3(A<float>(c, 1), A<float>(c, 2), A<float>(c, 3));
		e.rot = beamls::fromHeading(A<float>(c, 4));
		R<int>(c, world().addEntity(e));
	}
	H(DELETE_VEHICLE)
	{
		int *h = A<int *>(c, 0);
		if (auto *e = ent(*h))
			e->exists = false;
		*h = 0;
		count("DELETE_VEHICLE");
	}
	H(DOES_ENTITY_EXIST) { R<int>(c, ent(A<int>(c, 0)) ? 1 : 0); }
	H(GET_ENTITY_COORDS)
	{
		int h = A<int>(c, 0);
		if (h == world().playerPed && ent(world().playerVehicle))
			h = world().playerVehicle; // a ped in a vehicle is where the vehicle is
		const auto *e = ent(h);
		RV(c, e ? e->pos : V3());
	}
	H(GET_ENTITY_HEADING)
	{
		const auto *e = ent(A<int>(c, 0));
		R<float>(c, e ? beamls::headingOf(e->rot) : 0.0f);
	}
	H(GET_ENTITY_MODEL)
	{
		const auto *e = ent(A<int>(c, 0));
		R<std::uint32_t>(c, e ? e->model : 0u);
	}
	H(GET_ENTITY_QUATERNION)
	{
		const auto *e = ent(A<int>(c, 0));
		const Quat q = e ? e->rot : Quat();
		*A<float *>(c, 1) = q.x;
		*A<float *>(c, 2) = q.y;
		*A<float *>(c, 3) = q.z;
		*A<float *>(c, 4) = q.w;
	}
	H(GET_ENTITY_TYPE)
	{
		const auto *e = ent(A<int>(c, 0));
		R<int>(c, e ? e->type : 0);
	}
	H(GET_ENTITY_VELOCITY)
	{
		const auto *e = ent(A<int>(c, 0));
		RV(c, e ? e->vel : V3());
	}
	H(IS_ENTITY_IN_WATER)
	{
		const auto *e = ent(A<int>(c, 0));
		R<int>(c, e && e->inWater ? 1 : 0);
	}
	H(SET_ENTITY_ANGULAR_VELOCITY)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->angVel = V3(A<float>(c, 1), A<float>(c, 2), A<float>(c, 3));
	}
	H(SET_ENTITY_AS_MISSION_ENTITY)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->mission = A<int>(c, 1) != 0;
	}
	H(SET_ENTITY_COLLISION)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->collision = A<int>(c, 1) != 0;
	}
	H(SET_ENTITY_COORDS_NO_OFFSET)
	{
		if (auto *e = ent(A<int>(c, 0)))
		{
			e->pos = V3(A<float>(c, 1), A<float>(c, 2), A<float>(c, 3));
			++e->setCoordsCount;
		}
	}
	H(SET_ENTITY_INVINCIBLE)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->invincible = A<int>(c, 1) != 0;
	}
	H(SET_ENTITY_PROOFS) { (void)c; count("SET_ENTITY_PROOFS"); }
	H(SET_ENTITY_QUATERNION)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->rot = Quat(A<float>(c, 1), A<float>(c, 2), A<float>(c, 3), A<float>(c, 4));
	}
	H(SET_ENTITY_VELOCITY)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->vel = V3(A<float>(c, 1), A<float>(c, 2), A<float>(c, 3));
	}
	H(SET_ENTITY_VISIBLE)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->visible = A<int>(c, 1) != 0;
	}
	H(SET_VEHICLE_CAN_BE_VISIBLY_DAMAGED) { (void)c; }
	H(SET_VEHICLE_DOORS_LOCKED)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->doorLock = A<int>(c, 1);
	}
	H(SET_VEHICLE_ENGINE_ON) { (void)c; }
	H(SET_VEHICLE_FIXED)
	{
		if (auto *e = ent(A<int>(c, 0)))
			++e->fixedCount;
	}
	H(SET_VEHICLE_HAS_BEEN_OWNED_BY_PLAYER) { (void)c; }
	H(FORCE_USE_AUDIO_GAME_OBJECT)
	{
		if (auto *e = ent(A<int>(c, 0)))
			e->audio = A<const char *>(c, 1);
	}
	H(GET_VEHICLE_CLASS)
	{
		const auto *e = ent(A<int>(c, 0));
		R<int>(c, e ? e->vehicleClass : 0);
	}
	H(GET_PED_NEARBY_VEHICLES)
	{
		auto *arr = A<std::int64_t *>(c, 1);
		const std::int64_t cap = arr[0];
		int n = 0;
		for (int h : world().nearbyVehicles)
			if (ent(h) && n < cap)
				arr[1 + n++] = h;
		R<int>(c, n);
	}
	H(IS_PED_IN_VEHICLE) { R<int>(c, world().playerVehicle != 0 && world().playerVehicle == A<int>(c, 1) ? 1 : 0); }
	H(TASK_WARP_PED_INTO_VEHICLE)
	{
		world().playerVehicle = A<int>(c, 1);
		count("TASK_WARP_PED_INTO_VEHICLE");
	}
	// models
	H(REQUEST_MODEL)
	{
		const std::uint32_t m = A<std::uint32_t>(c, 0);
		if (world().requested.count(m) && world().modelDims.count(m))
			world().loaded.insert(m); // loads on the second request, like streaming takes a frame
		world().requested.insert(m);
	}
	H(HAS_MODEL_LOADED) { R<int>(c, world().loaded.count(A<std::uint32_t>(c, 0)) ? 1 : 0); }
	H(IS_MODEL_IN_CDIMAGE) { R<int>(c, world().modelDims.count(A<std::uint32_t>(c, 0)) ? 1 : 0); }
	H(SET_MODEL_AS_NO_LONGER_NEEDED) { (void)c; }
	H(GET_MODEL_DIMENSIONS)
	{
		auto it = world().modelDims.find(A<std::uint32_t>(c, 0));
		const V3 mn = it == world().modelDims.end() ? V3(-1, -1, -1) : it->second.first;
		const V3 mx = it == world().modelDims.end() ? V3(1, 1, 1) : it->second.second;
		outVec(c, 1, mn);
		outVec(c, 2, mx);
	}
	// camera
	H(GET_FINAL_RENDERED_CAM_COORD) { RV(c, world().camPos); }
	H(GET_FINAL_RENDERED_CAM_ROT) { RV(c, world().camRot); }
	H(GET_FINAL_RENDERED_CAM_FOV) { R<float>(c, world().camFov); }
	H(GET_FINAL_RENDERED_CAM_NEAR_CLIP) { R<float>(c, world().camNear); }
	H(INVALIDATE_IDLE_CAM) { (void)c; count("INVALIDATE_IDLE_CAM"); }
	// controls
	H(DISABLE_CONTROL_ACTION) { world().disabled.insert(A<int>(c, 1)); }
	H(GET_DISABLED_CONTROL_NORMAL)
	{
		auto it = world().normals.find(A<int>(c, 1));
		R<float>(c, it == world().normals.end() ? 0.0f : it->second);
	}
	H(IS_DISABLED_CONTROL_PRESSED) { R<int>(c, world().pressed.count(A<int>(c, 1)) ? 1 : 0); }
	H(IS_DISABLED_CONTROL_JUST_PRESSED) { R<int>(c, world().justPressed.count(A<int>(c, 1)) ? 1 : 0); }
	// state
	H(GET_FRAME_COUNT) { R<int>(c, world().frame); }
	H(GET_CLOCK_HOURS) { R<int>(c, world().hours); }
	H(GET_CLOCK_MINUTES) { R<int>(c, world().minutes); }
	H(GET_IS_LOADING_SCREEN_ACTIVE) { R<int>(c, world().loading); }
	H(IS_CUTSCENE_ACTIVE) { R<int>(c, world().cutscene); }
	H(IS_PLAYER_SWITCH_IN_PROGRESS) { R<int>(c, world().switching); }
	H(IS_SCREEN_FADED_OUT) { R<int>(c, world().faded); }
	H(IS_PAUSE_MENU_ACTIVE) { R<int>(c, world().pauseMenu); }
	H(IS_PLAYER_DEAD) { R<int>(c, world().playerDead); }
	H(PLAYER_ID) { R<int>(c, 0); }
	H(PLAYER_PED_ID) { R<int>(c, world().playerPed); }
	H(NETWORK_IS_SESSION_STARTED) { R<int>(c, world().sessionStarted); }
	H(NETWORK_IS_GAME_IN_PROGRESS) { R<int>(c, world().gameInProgress); }
	H(GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH)
	{
		auto it = world().scripts.find(A<std::uint32_t>(c, 0));
		R<int>(c, it == world().scripts.end() ? 0 : it->second);
	}
	// shape tests
	H(START_SHAPE_TEST_LOS_PROBE)
	{
		fake::World::Probe p;
		p.from = V3(A<float>(c, 0), A<float>(c, 1), A<float>(c, 2));
		p.to = V3(A<float>(c, 3), A<float>(c, 4), A<float>(c, 5));
		p.flags = A<int>(c, 6);
		p.ignore = A<int>(c, 7);
		p.frame = world().frame;
		const int h = world().nextProbe++;
		world().probes[h] = p;
		++world().probesStarted;
		R<int>(c, h);
	}
	H(GET_SHAPE_TEST_RESULT)
	{
		auto it = world().probes.find(A<int>(c, 0));
		if (it == world().probes.end())
		{
			R<int>(c, 0);
			return;
		}
		const auto p = it->second;
		if (p.frame == world().frame)
		{
			R<int>(c, 1); // results come a frame later
			return;
		}
		world().probes.erase(it);
		float best = 2.0f;
		V3 normal(0, 0, 1);
		int hitEntity = 0;
		const V3 d = p.to - p.from;
		if (p.flags & 1)
		{
			// ground: march the segment
			const int steps = 200;
			float prev = p.from.z - world().groundAt(p.from.x, p.from.y);
			for (int i = 1; i <= steps; ++i)
			{
				const float t = float(i) / steps;
				const V3 q = p.from + d * t;
				const float h = q.z - world().groundAt(q.x, q.y);
				if (prev > 0.0f && h <= 0.0f)
				{
					const float tt = (float(i - 1) + prev / (prev - h)) / steps;
					if (tt < best)
					{
						best = tt;
						normal = V3(-world().groundSlopeX, 0, 1).norm();
					}
					break;
				}
				prev = h;
			}
			for (const auto &w : world().walls)
			{
				float t;
				V3 n;
				if (segBox(p.from, p.to, w.min, w.max, t, n) && t < best)
				{
					best = t;
					normal = n;
					hitEntity = 0;
				}
			}
		}
		if (p.flags & 16)
		{
			for (const auto &kv : world().entities)
			{
				const auto &e = kv.second;
				if (!e.exists || e.type != 3 || kv.first == p.ignore)
					continue;
				const auto &dm = world().modelDims[e.model];
				float t;
				V3 n;
				if (segBox(p.from, p.to, e.pos + dm.first, e.pos + dm.second, t, n) && t < best)
				{
					best = t;
					normal = n;
					hitEntity = kv.first;
				}
			}
		}
		const bool hit = best <= 1.0f;
		*A<int *>(c, 1) = hit ? 1 : 0;
		outVec(c, 2, hit ? p.from + d * best : V3());
		outVec(c, 3, hit ? normal : V3());
		*A<int *>(c, 4) = hit ? hitEntity : 0;
		R<int>(c, 2);
	}
#undef H

	struct Reg
	{
		const char *name;
		beamls::NativeHandler h;
	};
#define E(name) {#name, &h_##name}
	const Reg kRegistry[] = {
		E(ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME), E(BEGIN_TEXT_COMMAND_DISPLAY_HELP), E(BEGIN_TEXT_COMMAND_THEFEED_POST),
		E(CREATE_VEHICLE), E(DELETE_VEHICLE), E(DISABLE_CONTROL_ACTION), E(DOES_ENTITY_EXIST), E(END_TEXT_COMMAND_DISPLAY_HELP),
		E(END_TEXT_COMMAND_THEFEED_POST_TICKER), E(FORCE_USE_AUDIO_GAME_OBJECT), E(GET_CLOCK_HOURS), E(GET_CLOCK_MINUTES),
		E(GET_DISABLED_CONTROL_NORMAL), E(GET_ENTITY_COORDS), E(GET_ENTITY_HEADING), E(GET_ENTITY_MODEL), E(GET_ENTITY_QUATERNION),
		E(GET_ENTITY_TYPE), E(GET_ENTITY_VELOCITY), E(GET_FINAL_RENDERED_CAM_COORD), E(GET_FINAL_RENDERED_CAM_FOV),
		E(GET_FINAL_RENDERED_CAM_NEAR_CLIP), E(GET_FINAL_RENDERED_CAM_ROT), E(GET_FRAME_COUNT), E(GET_IS_LOADING_SCREEN_ACTIVE),
		E(GET_MODEL_DIMENSIONS), E(GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH), E(GET_PED_NEARBY_VEHICLES),
		E(GET_SHAPE_TEST_RESULT), E(GET_VEHICLE_CLASS), E(HAS_MODEL_LOADED), E(INVALIDATE_IDLE_CAM), E(IS_CUTSCENE_ACTIVE),
		E(IS_DISABLED_CONTROL_JUST_PRESSED), E(IS_DISABLED_CONTROL_PRESSED), E(IS_ENTITY_IN_WATER), E(IS_MODEL_IN_CDIMAGE),
		E(IS_PAUSE_MENU_ACTIVE), E(IS_PED_IN_VEHICLE), E(IS_PLAYER_DEAD), E(IS_PLAYER_SWITCH_IN_PROGRESS), E(IS_SCREEN_FADED_OUT),
		E(NETWORK_IS_GAME_IN_PROGRESS), E(NETWORK_IS_SESSION_STARTED), E(PLAYER_ID), E(PLAYER_PED_ID), E(REQUEST_MODEL),
		E(SET_ENTITY_ANGULAR_VELOCITY), E(SET_ENTITY_AS_MISSION_ENTITY), E(SET_ENTITY_COLLISION), E(SET_ENTITY_COORDS_NO_OFFSET),
		E(SET_ENTITY_INVINCIBLE), E(SET_ENTITY_PROOFS), E(SET_ENTITY_QUATERNION), E(SET_ENTITY_VELOCITY), E(SET_ENTITY_VISIBLE),
		E(SET_MODEL_AS_NO_LONGER_NEEDED), E(SET_VEHICLE_CAN_BE_VISIBLY_DAMAGED), E(SET_VEHICLE_DOORS_LOCKED), E(SET_VEHICLE_ENGINE_ON),
		E(SET_VEHICLE_FIXED), E(SET_VEHICLE_HAS_BEEN_OWNED_BY_PLAYER), E(START_SHAPE_TEST_LOS_PROBE), E(TASK_WARP_PED_INTO_VEHICLE),
	};
#undef E
}

namespace beamls
{
	bool nativesReady() { return true; }

	NativeHandler handlerFor(std::uint64_t original)
	{
		static std::unordered_map<std::uint64_t, NativeHandler> map = [] {
			std::unordered_map<std::string, NativeHandler> byName;
			for (const auto &r : kRegistry)
				byName[r.name] = r.h;
			std::unordered_map<std::uint64_t, NativeHandler> m;
			const auto &t = gen::kBuildTables[0];
			for (std::size_t i = 0; i < t.count; ++i)
			{
				auto it = byName.find(t.entries[i].name);
				m[t.entries[i].original] = it == byName.end() ? nullptr : it->second;
			}
			return m;
		}();
		auto it = map.find(original);
		return it == map.end() ? nullptr : it->second;
	}

	// Every native in the sheet must have a fake (the tests check this).
	int fakeNativesMissing()
	{
		int missing = 0;
		const auto &t = gen::kBuildTables[0];
		for (std::size_t i = 0; i < t.count; ++i)
			if (!handlerFor(t.entries[i].original))
				++missing;
		return missing;
	}
}
