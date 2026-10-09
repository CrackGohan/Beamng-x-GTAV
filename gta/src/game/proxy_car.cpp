// The invisible GTA car that stands in for the BeamNG car: sized like it (sheet player_proxies), kept on its
// pose and velocity every frame, so traffic, peds and police react to it and the player can get in and out.
// (sheet systems: gta_proxy_car)
#include "game.hpp"
#include "../core/log.hpp"
#include "../gen/natives.hpp"
#include "../gen/player_proxies.hpp"
#include "../gen/settings.hpp"

namespace beamls::game::proxy
{
	namespace
	{
		const gen::PlayerProxyRow &pick(const msg::VehicleInfo &info)
		{
			for (const auto &r : gen::kPlayerProxies)
				if (info.length >= r.minLength && info.length < r.maxLength && info.height >= r.minHeight && info.height < r.maxHeight)
					return r;
			return gen::kPlayerProxies[sizeof(gen::kPlayerProxies) / sizeof(gen::kPlayerProxies[0]) - 1];
		}

		const gen::PlayerProxyRow &fallback() { return gen::kPlayerProxies[sizeof(gen::kPlayerProxies) / sizeof(gen::kPlayerProxies[0]) - 1]; }

		void setup(Game &g, Vehicle v)
		{
			ENTITY::SET_ENTITY_AS_MISSION_ENTITY(v, TRUE, TRUE);
			ENTITY::SET_ENTITY_VISIBLE(v, FALSE, FALSE);
			ENTITY::SET_ENTITY_COLLISION(v, TRUE, TRUE);
			ENTITY::SET_ENTITY_INVINCIBLE(v, TRUE, FALSE);
			ENTITY::SET_ENTITY_PROOFS(v, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE);
			VEHICLE::SET_VEHICLE_CAN_BE_VISIBLY_DAMAGED(v, FALSE);
			VEHICLE::SET_VEHICLE_DOORS_LOCKED(v, 1);
			VEHICLE::SET_VEHICLE_HAS_BEEN_OWNED_BY_PLAYER(v, TRUE);
			VEHICLE::SET_VEHICLE_ENGINE_ON(v, TRUE, TRUE, FALSE);
			AUDIO::FORCE_USE_AUDIO_GAME_OBJECT(v, settings::proxy_audio);
			Vector3 mn, mx;
			MISC::GET_MODEL_DIMENSIONS(g.proxy.model, &mn, &mx);
			g.proxy.modelCenter = V3((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
			g.proxy.modelSize = V3(mx.x - mn.x, mx.y - mn.y, mx.z - mn.z);
		}

		// Model origin that puts the model's box centre on the BeamNG car's box centre.
		V3 originFor(Game &g, const V3 &centre, const Quat &rot) { return centre - rot.rotate(g.proxy.modelCenter); }
	}

	bool exists(Game &g) { return g.proxy.handle != 0 && ENTITY::DOES_ENTITY_EXIST(g.proxy.handle); }

	void destroy(Game &g)
	{
		if (g.proxy.handle != 0 && ENTITY::DOES_ENTITY_EXIST(g.proxy.handle))
		{
			Vehicle v = g.proxy.handle;
			ENTITY::SET_ENTITY_AS_MISSION_ENTITY(v, TRUE, TRUE);
			VEHICLE::DELETE_VEHICLE(&v);
		}
		g.proxy = Proxy();
	}

	void requestSpawnNextToPlayer(Game &g)
	{
		const Vector3 p = ENTITY::GET_ENTITY_COORDS(g.player, TRUE);
		const float heading = ENTITY::GET_ENTITY_HEADING(g.player);
		const Quat q = fromHeading(heading);
		const V3 right = q.rotate(V3(1, 0, 0));
		g.spawnPos = V3(p.x, p.y, p.z) + right * settings::spawn_side_offset;
		g.spawnHeading = heading;
		g.spawnPending = true;
		g.spawnAsked = -1e9;
		g.spawnStarted = g.now;
		log::info("asking BeamNG to place the car at %.1f %.1f %.1f", g.spawnPos.x, g.spawnPos.y, g.spawnPos.z);
	}

	void onVehicleInfo(Game &g, const msg::VehicleInfo &info)
	{
		const bool first = !g.haveCarInfo;
		g.carInfo = info;
		g.haveCarInfo = true;
		const auto &row = pick(info);
		log::info("BeamNG car: %s (%s) %.1f x %.1f x %.1f m -> GTA proxy %s", info.name.c_str(), info.model.c_str(),
		          info.length, info.width, info.height, row.model);
		if (!first && g.proxy.handle != 0 && row.modelHash != g.proxy.model)
		{
			// The car changed size class: rebuild the proxy, keeping the player in it.
			g.proxy.warpPlayerIn = g.playerInProxy;
			const bool warp = g.proxy.warpPlayerIn;
			destroy(g);
			g.proxy.warpPlayerIn = warp;
		}
	}

	void update(Game &g)
	{
		if (!g.carChosen)
			return;
		// 1. Placing the car next to the player after the first choice.
		if (g.spawnPending)
		{
			if (g.now - g.spawnAsked > 0.5)
			{
				msg::SpawnAt s;
				s.x = g.spawnPos.x;
				s.y = g.spawnPos.y;
				s.z = g.spawnPos.z;
				s.heading = g.spawnHeading;
				g.bridge.send(s);
				g.spawnAsked = g.now;
			}
			if (g.bridge.hasCar)
			{
				const V3 d = g.carCentre() - g.spawnPos;
				if (d.x * d.x + d.y * d.y < 10.0f * 10.0f)
				{
					g.spawnPending = false;
					log::info("BeamNG car placed");
				}
			}
			if (g.spawnPending && g.now - g.spawnStarted > 10.0)
			{
				log::warn("BeamNG did not place the car in 10 s");
				hud::notify(g, "BeamNG did not place the car. Press F6 to try again.");
				g.spawnPending = false;
				g.carChosen = false;
			}
			return;
		}
		if (!g.bridge.hasCar || !g.haveCarInfo)
			return;

		const V3 centre = g.carCentre();
		const Quat rot = g.carRot();
		// 2. Create the proxy when there is none.
		if (!exists(g))
		{
			const auto &row = pick(g.carInfo);
			Hash model = row.modelHash;
			if (!STREAMING::IS_MODEL_IN_CDIMAGE(model))
				model = fallback().modelHash;
			if (g.proxy.model != model)
			{
				const bool warp = g.proxy.warpPlayerIn; // survives the reset: the player goes back in
				g.proxy = Proxy();
				g.proxy.warpPlayerIn = warp;
				g.proxy.model = model;
				g.proxy.sizeClass = row.id;
				g.proxy.modelRequested = g.now;
			}
			STREAMING::REQUEST_MODEL(model);
			if (!STREAMING::HAS_MODEL_LOADED(model))
			{
				if (g.now - g.proxy.modelRequested > 5.0 && model != fallback().modelHash)
					g.proxy.model = fallback().modelHash;
				return;
			}
			const V3 o = centre - rot.rotate(V3(0, 0, 0));
			const Vehicle v = VEHICLE::CREATE_VEHICLE(model, o.x, o.y, o.z, headingOf(rot), FALSE, FALSE, FALSE);
			if (v == 0)
				return;
			g.proxy.handle = v;
			setup(g, v);
			STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
			log::info("proxy car %d created (%s)", v, g.proxy.sizeClass);
			if (g.proxy.warpPlayerIn)
			{
				TASK::TASK_WARP_PED_INTO_VEHICLE(g.player, v, -1);
				g.proxy.warpPlayerIn = false;
			}
		}

		// 3. Keep it on the BeamNG car.
		const Vehicle v = g.proxy.handle;
		const V3 o = originFor(g, centre, rot);
		const V3 vel = g.carVel();
		const auto &c = g.bridge.car;
		ENTITY::SET_ENTITY_COORDS_NO_OFFSET(v, o.x, o.y, o.z, FALSE, FALSE, FALSE);
		ENTITY::SET_ENTITY_QUATERNION(v, rot.x, rot.y, rot.z, rot.w);
		ENTITY::SET_ENTITY_VELOCITY(v, vel.x, vel.y, vel.z);
		ENTITY::SET_ENTITY_ANGULAR_VELOCITY(v, c.wx, c.wy, c.wz);
		if (g.now - g.proxy.lastRefresh > 1.0)
		{
			// Game scripts can reset these; keep the stand-in invisible and unbreakable.
			g.proxy.lastRefresh = g.now;
			ENTITY::SET_ENTITY_VISIBLE(v, FALSE, FALSE);
			VEHICLE::SET_VEHICLE_FIXED(v);
			if (ENTITY::IS_ENTITY_IN_WATER(v))
				log::info("proxy in water (BeamNG handles floating and sinking)");
		}
	}
}
