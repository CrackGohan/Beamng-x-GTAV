// BeamLS's GTA-side game logic, ticked against the fake GTA, talking over real UDP to a fake BeamNG peer
// written here. Plays a whole session: connect, F6, choose a car, spawn next to the player, proxy car,
// driving input, ground / wall / traffic probing, repair, time, gating, size change, F8, BeamNG leaving,
// and GTA Online shutting everything down.
#include "../src/core/json.hpp"
#include "../src/core/log.hpp"
#include "../src/core/platform.hpp"
#include "../src/game/composite_state.hpp"
#include "../src/game/game.hpp"
#include "../src/gen/player_proxies.hpp"
#include "../src/gen/settings.hpp"
#include "check.hpp"
#include "fake_gta.hpp"
#include <map>
#include <string>
#include <vector>

using namespace beamls;

struct FakeBeamNG
{
	platform::Udp udp;
	int gtaPort = 0;
	std::vector<json::Value> got;

	void poll()
	{
		char buf[65536];
		for (;;)
		{
			int from = 0;
			const int n = udp.recv(buf, sizeof(buf), from);
			if (n <= 0)
				break;
			gtaPort = from;
			json::Value v;
			if (json::parse(buf, std::size_t(n), v))
				got.push_back(v);
		}
	}
	void send(const std::string &s) { udp.sendTo(gtaPort, s); }
	// Like the real BeamNG: a status every 10 frames and the car every frame while one exists.
	bool alive = false;
	std::string lastCar;
	int frames = 0;
	void heartbeat()
	{
		if (!alive || gtaPort == 0)
			return;
		if (++frames % 10 == 0)
			send("{\"t\":\"status\",\"state\":\"ready\",\"msg\":\"ok\"}");
		if (!lastCar.empty())
			send(lastCar);
	}
	std::vector<const json::Value *> of(const char *t) const
	{
		std::vector<const json::Value *> out;
		for (const auto &v : got)
			if (v.get("t") && v.get("t")->str() == t)
				out.push_back(&v);
		return out;
	}
	const json::Value *last(const char *t) const
	{
		auto v = of(t);
		return v.empty() ? nullptr : v.back();
	}
	double num(const json::Value *v, const char *k) const { return v && v->get(k) ? v->get(k)->num() : -12345; }
	std::string str(const json::Value *v, const char *k) const { return v && v->get(k) ? v->get(k)->str() : std::string(); }
	void clear() { got.clear(); }

	void sendCar(float x, float y, float z, float heading, float vx = 0, float vy = 0, float speed = 0)
	{
		const Quat q = fromHeading(heading);
		char buf[1024];
		std::snprintf(buf, sizeof(buf),
		              "{\"t\":\"car\",\"seq\":%u,\"cam_seq\":1,\"x\":%f,\"y\":%f,\"z\":%f,\"qx\":%f,\"qy\":%f,\"qz\":%f,\"qw\":%f,"
		              "\"vx\":%f,\"vy\":%f,\"vz\":0,\"wx\":0,\"wy\":0,\"wz\":0,\"bx\":%f,\"by\":%f,\"bz\":%f,\"hx\":0.95,\"hy\":2.35,\"hz\":0.72,"
		              "\"speed\":%f,\"damage\":0}",
		              ++carSeq, x, y, z, q.x, q.y, q.z, q.w, vx, vy, x, y, z + 0.6f, speed);
		lastCar = buf;
		send(buf);
	}
	unsigned carSeq = 0;
};

static FakeBeamNG bng;

// One game frame: BeamNG answers first (like a parallel process), then GTA ticks.
static void frame(int n = 1)
{
	for (int i = 0; i < n; ++i)
	{
		fake::nextFrame();
		bng.poll();
		bng.heartbeat();
		game::tick();
	}
	bng.poll();
}

static bool helpShown(const std::string &needle)
{
	for (const auto &h : fake::world().helps)
		if (h.find(needle) != std::string::npos)
			return true;
	return false;
}

static bool feedShown(const std::string &needle)
{
	for (const auto &h : fake::world().feed)
		if (h.find(needle) != std::string::npos)
			return true;
	return false;
}

int main()
{
	log::open("build/tests/test_game.log");
	fake::reset();
	auto &w = fake::world();
	auto &g = game::G();
	CHECK(bng.udp.open());
	g.bridge.start("3889", 99);
	g.bridge.setPort(bng.udp.localPort());

	// 1. Waiting for BeamNG: hello once a second, nothing else.
	frame(130);
	CHECK(bng.of("hello").size() >= 2 && bng.of("hello").size() <= 3);
	CHECK(bng.num(bng.last("hello"), "proto") == 1 && bng.str(bng.last("hello"), "gta_build") == "3889");
	CHECK(bng.of("cam").empty());
	CHECK(g.mode == game::Mode::WaitBeamNG);

	// 2. BeamNG answers, ready.
	bng.send("{\"t\":\"hello_ack\",\"proto\":1,\"bng_version\":\"0.39.4.0\",\"session\":99,\"ready\":true}");
	bng.alive = true;
	frame(2);
	CHECK(g.bridge.connected() && g.bridge.ready());
	CHECK(!bng.of("cam").empty());
	CHECK(helpShown("F6"));
	const json::Value *cam = bng.last("cam");
	CHECK(check::near(bng.num(cam, "px"), 0) && check::near(bng.num(cam, "py"), -6) && check::near(bng.num(cam, "fov"), 50));
	CHECK(bng.num(cam, "w") == 1920 && bng.num(cam, "h") == 1080);
	CHECK(w.calls["INVALIDATE_IDLE_CAM"] > 0);
	CHECK(bng.str(bng.last("mode"), "state") == "onfoot");

	// 3. F6: vehicle selector in BeamNG.
	bng.clear();
	fake::platform().keys.insert(0x75);
	frame(1);
	fake::platform().keys.clear();
	frame(1);
	CHECK(bng.str(bng.last("mode"), "state") == "menu_vehicle");
	CHECK(fake::platform().focusBeamNGCalls == 1);
	CHECK(g.menuOpen);
	// keys are ignored while BeamNG has focus
	fake::platform().keys.insert(0x75);
	frame(1);
	fake::platform().keys.clear();
	CHECK(g.menuOpen);

	// 4. The player picks a sedan and returns: the car is placed 3.5 m to the player's right.
	w.entities[w.playerPed].pos = V3(100, 200, 15);
	w.entities[w.playerPed].rot = fromHeading(0);
	bng.send("{\"t\":\"vehicle_info\",\"model\":\"etk800\",\"config\":\"base\",\"name\":\"ETK 800\",\"length\":4.7,\"width\":1.9,\"height\":1.45,\"mass\":0}");
	bng.send("{\"t\":\"menu_closed\",\"changed\":true}");
	frame(2);
	CHECK(fake::platform().focusGtaCalls == 1);
	CHECK(!g.menuOpen && g.carChosen && g.spawnPending);
	const json::Value *sp = bng.last("spawn_at");
	CHECK(sp && check::near(bng.num(sp, "x"), 103.5, 1e-3) && check::near(bng.num(sp, "y"), 200, 1e-3) && check::near(bng.num(sp, "heading"), 0));
	// BeamNG places the car; GTA builds the invisible proxy (sedan class -> fugitive)
	bng.sendCar(103.5f, 200, 15.1f, 0);
	frame(4);
	CHECK(!g.spawnPending);
	CHECK(g.proxy.handle != 0);
	const auto *proxy = w.entities.count(g.proxy.handle) ? &w.entities[g.proxy.handle] : nullptr;
	CHECK(proxy && proxy->model == fake::joaat("fugitive"));
	CHECK(proxy && !proxy->visible && proxy->mission && proxy->invincible && proxy->collision);
	CHECK(proxy && proxy->audio == settings::proxy_audio);
	// proxy origin = BeamNG box centre - model box centre (fugitive centre is 0.15 m up)
	CHECK(proxy && check::near(proxy->pos.x, 103.5, 1e-3) && check::near(proxy->pos.z, 15.1 + 0.6 - 0.15, 1e-3));
	// 5. The composite is drawn once a car exists and the player is on foot or driving.
	{
		const auto s = composite::read();
		CHECK(s.draw);
		const auto *c = composite::newestCar(s);
		CHECK(c && check::near(c->half.y, 2.35));
		CHECK(composite::newestCamera(s) != nullptr);
	}

	// 6. Ground probing: grid aligned to the step, heights from GTA's ground (z = 10 + 0.05 x).
	bng.clear();
	frame(30);
	const json::Value *gr = bng.last("ground");
	CHECK(gr != nullptr);
	if (gr)
	{
		const double ox = bng.num(gr, "ox"), oy = bng.num(gr, "oy"), step = bng.num(gr, "step");
		const int n = int(bng.num(gr, "n"));
		CHECK(n == settings::ground_n && check::near(step, settings::ground_step));
		CHECK(check::near(std::fmod(ox, step), 0) && check::near(std::fmod(oy, step), 0));
		const auto &z = gr->get("z")->arr();
		CHECK(int(z.size()) == n * n);
		int good = 0;
		for (int j = 0; j < n; ++j)
			for (int i = 0; i < n; ++i)
				if (check::near(z[std::size_t(j * n + i)].num(), 10.0 + 0.05 * (ox + i * step), 0.05))
					++good;
		CHECK(good == n * n);
	}
	CHECK(bng.of("walls").empty()); // wall rings only while driving

	// 7. The player gets in: drive mode, GTA's controls disabled and forwarded.
	w.playerVehicle = g.proxy.handle;
	w.normals[71] = 0.75f;
	w.normals[59] = -0.5f;
	w.pressed.insert(76);
	bng.clear();
	frame(3);
	CHECK(g.mode == game::Mode::Drive);
	CHECK(bng.str(bng.last("mode"), "state") == "drive");
	const json::Value *in = bng.last("input");
	CHECK(in && check::near(bng.num(in, "throttle"), 0.75) && check::near(bng.num(in, "steer"), -0.5) && check::near(bng.num(in, "handbrake"), 1));
	CHECK(w.disabled.count(71) && w.disabled.count(72) && w.disabled.count(59) && w.disabled.count(76));
	w.normals.clear();
	w.pressed.clear();

	// 8. Walls and props around the car.
	w.walls.push_back({V3(110, 190, 0), V3(111, 210, 40)});              // a wall 6.5 m east of the car
	fake::Entity thin;
	thin.type = 3;
	thin.model = fake::joaat("prop_bollard_02a");
	thin.pos = V3(103.5f, 208, 15.2f);                                     // a bollard 8 m north
	w.addEntity(thin);
	fake::Entity big;
	big.type = 3;
	big.model = fake::joaat("prop_dumpster_01a");
	big.pos = V3(103.5f, 190, 15.2f);                                      // a dumpster 10 m south
	w.addEntity(big);
	bng.clear();
	frame(40);
	bool sawWall = false, sawDumpster = false, sawBollard = false;
	for (const auto *m : bng.of("walls"))
		for (const auto &h : m->get("hits")->arr())
		{
			const auto &c = h.arr();
			if (c[6].num() == 1 && check::near(c[0].num(), 110, 0.05) && check::near(c[3].num(), -1))
				sawWall = true;
			if (c[6].num() == 2 && c[7].num() > 0.5)
				sawDumpster = true;
			if (c[6].num() == 2 && c[7].num() < 0.25)
				sawBollard = true;
		}
	CHECK(sawWall);
	CHECK(sawDumpster);
	CHECK(!sawBollard); // thin props break off in GTA: not mirrored
	CHECK(w.probesStarted > 0);

	// 9. Traffic: a police car 9 m away and a far car; the proxy itself is never listed.
	fake::Entity cop;
	cop.model = fake::joaat("police");
	cop.pos = V3(103.5f, 209, 15.1f);
	cop.vel = V3(0, -12, 0);
	cop.rot = fromHeading(180);
	cop.vehicleClass = 18;
	const int copHandle = w.addEntity(cop);
	fake::Entity far;
	far.model = fake::joaat("fugitive");
	far.pos = V3(400, 200, 15);
	const int farHandle = w.addEntity(far);
	w.nearbyVehicles = {g.proxy.handle, copHandle, farHandle};
	bng.clear();
	frame(5);
	const json::Value *tr = bng.last("traffic");
	CHECK(tr && tr->get("cars")->arr().size() == 1);
	if (tr && !tr->get("cars")->arr().empty())
	{
		const auto &c = tr->get("cars")->arr()[0].arr();
		CHECK(c[0].num() == copHandle && c[1].num() == 18);
		CHECK(check::near(c[3].num(), 209 + 0, 0.01));            // box centre y (police centre y offset 0)
		CHECK(check::near(c[4].num(), 15.1 + 0.2, 0.01));         // box centre z: (min.z + max.z) / 2 = 0.2
		CHECK(check::near(c[10].num(), -12) && check::near(c[12].num(), 5.0) && check::near(c[13].num(), 1.9));
	}

	// 10. The proxy follows the BeamNG car (with velocity).
	bng.sendCar(103.5f, 230, 15.1f, 0, 0, 20, 20);
	frame(1);
	proxy = &w.entities[g.proxy.handle];
	CHECK(proxy->pos.y > 229.9f && proxy->pos.y < 230.6f);
	CHECK(check::near(proxy->vel.y, 20));

	// 11. Los Santos Customs Burton: stop at the door, press E.
	bng.sendCar(-337.39f, -136.92f, 38.5f, 0);
	frame(2);
	CHECK(helpShown("Los Santos Customs, Burton"));
	CHECK(w.disabled.count(51));
	w.justPressed.insert(51);
	g.now += 0; // same frame
	bng.clear();
	game::tick(); // the press is seen this frame
	bng.poll();
	CHECK(bng.str(bng.last("repair"), "garage") == "lsc_burton");
	frame(1);
	CHECK(helpShown("Repaired"));

	// 12. Time sync.
	w.hours = 21;
	w.minutes = 5;
	bng.clear();
	frame(int(60 * settings::time_rate_s) + 5);
	CHECK(bng.num(bng.last("time"), "hour") == 21 && bng.num(bng.last("time"), "minute") == 5);

	// 13. A cutscene pauses everything: mode paused, no probing, no composite.
	w.cutscene = true;
	bng.clear();
	const int probesBefore = w.probesStarted;
	frame(20);
	CHECK(bng.str(bng.last("mode"), "state") == "paused");
	CHECK(bng.of("cam").empty() && bng.of("input").empty());
	CHECK(w.probesStarted == probesBefore);
	CHECK(!composite::read().draw);
	w.cutscene = false;
	frame(2);

	// 14. The player changes to a pickup in BeamNG (F7 tuning): the proxy is rebuilt and the player put back in.
	fake::platform().keys.insert(0x76);
	frame(1);
	fake::platform().keys.clear();
	CHECK(bng.str(bng.last("mode"), "state") == "menu_tuning");
	const int oldProxy = g.proxy.handle;
	bng.send("{\"t\":\"vehicle_info\",\"model\":\"pickup\",\"config\":\"crew\",\"name\":\"D-Series\",\"length\":5.6,\"width\":2.0,\"height\":1.9,\"mass\":0}");
	bng.send("{\"t\":\"menu_closed\",\"changed\":true}");
	frame(6);
	CHECK(g.proxy.handle != 0 && g.proxy.handle != oldProxy);
	CHECK(!w.entities[oldProxy].exists);
	CHECK(w.entities[g.proxy.handle].model == fake::joaat("bison"));
	CHECK(w.playerVehicle == g.proxy.handle);
	CHECK(w.calls["TASK_WARP_PED_INTO_VEHICLE"] == 1);

	// 15. F8 switches the mashup off (proxy gone) and on again (proxy back).
	fake::platform().keys.insert(0x77);
	frame(1);
	fake::platform().keys.clear();
	frame(1);
	CHECK(g.userOff && g.proxy.handle == 0);
	CHECK(bng.str(bng.last("mode"), "state") == "off");
	CHECK(!composite::read().draw);
	fake::platform().keys.insert(0x77);
	frame(1);
	fake::platform().keys.clear();
	frame(4);
	CHECK(!g.userOff && g.proxy.handle != 0);

	// 16. BeamNG closes: the car is gone in GTA too, and hello starts again.
	bng.send("{\"t\":\"bye\",\"reason\":\"quit\"}");
	bng.alive = false;
	bng.lastCar.clear();
	const int proxyAtBye = g.proxy.handle;
	frame(2);
	CHECK(!g.carChosen && g.proxy.handle == 0 && !w.entities[proxyAtBye].exists);
	bng.clear();
	frame(70);
	CHECK(feedShown("BeamNG closed")); // notices queue: one every half second
	CHECK(!bng.of("hello").empty());

	// 17. BeamNG comes back; GTA Online starts: everything is removed and BeamLS stays off.
	bng.send("{\"t\":\"hello_ack\",\"proto\":1,\"bng_version\":\"0.39.4.0\",\"session\":99,\"ready\":true}");
	bng.alive = true;
	frame(2);
	CHECK(g.bridge.connected());
	w.scripts[fake::joaat("freemode")] = 1;
	bng.clear();
	frame(1);
	CHECK(g.online);
	CHECK(bng.str(bng.last("bye"), "reason") == "online");
	CHECK(feedShown("story mode only"));
	bng.clear();
	frame(120);
	bng.alive = false;
	bng.clear();
	frame(60);
	CHECK(bng.got.empty()); // silent for good
	CHECK(!composite::read().draw);

	// 18. Wrong protocol from BeamNG is refused (fresh bridge).
	{
		Bridge b;
		b.start("3889", 1);
		b.setPort(bng.udp.localPort());
		b.poll(100.0);
		bng.poll();
		bng.send("{\"t\":\"hello_ack\",\"proto\":2,\"bng_version\":\"x\",\"session\":1,\"ready\":true}");
		b.poll(100.1);
		CHECK(!b.connected());
		bng.send("not json");
		bng.send("{\"no_type\":1}");
		b.poll(100.2);
		CHECK(b.malformed() == 2);
	}
	return check::summary("gta game tests");
}
