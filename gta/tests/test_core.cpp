// Core pieces of BeamLS.asi on Linux: JSON, the generated protocol, version support, pattern scanning,
// rotation maths and the invoker's argument/vector handling (against fake handlers).
#include "../src/core/invoker.hpp"
#include "../src/core/json.hpp"
#include "../src/core/mathx.hpp"
#include "../src/core/patterns.hpp"
#include "../src/core/version.hpp"
#include "../src/gen/natives.hpp"
#include "../src/gen/protocol.hpp"
#include "check.hpp"
#include "fake_gta.hpp"
#include <cstring>
#include <fstream>
#include <vector>

namespace beamls
{
	int fakeNativesMissing();
}
using namespace beamls;

static json::Value parse(const std::string &s)
{
	json::Value v;
	CHECK(json::parse(s.data(), s.size(), v));
	return v;
}

int main(int argc, char **argv)
{
	// --- JSON -------------------------------------------------------------------------------------------
	{
		json::Value v;
		const std::string doc = "{\"a\":1.5,\"b\":[1,2,[3]],\"c\":\"x\\ny\\u00e9\",\"d\":true,\"e\":null}";
		CHECK(json::parse(doc.data(), doc.size(), v));
		CHECK(v.get("a") && check::near(v.get("a")->num(), 1.5));
		CHECK(v.get("b") && v.get("b")->arr().size() == 3 && v.get("b")->arr()[2].arr()[0].num() == 3);
		CHECK(v.get("c") && v.get("c")->str() == "x\ny\xC3\xA9");
		CHECK(v.get("d") && v.get("d")->boolean());
		CHECK(v.get("e") && v.get("e")->type == json::Value::Type::Null);
		for (const char *bad : {"{", "{\"a\":}", "[1,2", "{\"a\":1}x", "nul", "{\"a\" 1}"})
			CHECK(!json::parse(bad, std::strlen(bad), v));
		std::string deep(100, '[');
		CHECK(!json::parse(deep.data(), deep.size(), v)); // depth limit, no stack overflow
	}

	// --- Protocol round trips -------------------------------------------------------------------------------
	{
		msg::Cam c;
		c.seq = 42;
		c.px = -1200.5f;
		c.py = 300.25f;
		c.pz = 35.0f;
		c.qx = 0.1f;
		c.qy = 0.2f;
		c.qz = 0.3f;
		c.qw = 0.927f;
		c.fov = 50.0f;
		c.near = 0.15f;
		c.w = 2560;
		c.h = 1440;
		const std::string s = c.encode();
		const json::Value v = parse(s);
		CHECK(v.get("t") && v.get("t")->str() == "cam");
		msg::Cam d;
		CHECK(d.decode(v));
		CHECK(d.seq == 42 && check::near(d.px, -1200.5) && check::near(d.py, 300.25) && d.w == 2560 && d.h == 1440 && check::near(d.near, 0.15));

		msg::Walls w;
		w.seq = 7;
		w.hits.push_back({1, 2, 3, -1, 0, 0, 1, 0});
		w.hits.push_back({4, 5, 6, 0, 1, 0, 2, 0.6f});
		msg::Walls w2;
		CHECK(w2.decode(parse(w.encode())));
		CHECK(w2.hits.size() == 2 && w2.hits[1].kind == 2 && check::near(w2.hits[1].size, 0.6) && check::near(w2.hits[0].nx, -1));

		msg::Ground g;
		g.n = 3;
		g.step = 2;
		g.z = {1, 2, 3, 4, 5, 6, 7, 8, -10000};
		msg::Ground g2;
		CHECK(g2.decode(parse(g.encode())));
		CHECK(g2.z.size() == 9 && check::near(g2.z[8], -10000) && g2.n == 3);

		msg::Hello h;
		h.proto = msg::kProto;
		h.gta_build = "3889\"quoted\\";
		msg::Hello h2;
		CHECK(h2.decode(parse(h.encode())));
		CHECK(h2.gta_build == h.gta_build);

		// what BeamNG's Lua side sends (gen/protocol.lua encode + jsonEncode)
		const std::string fromLua = "{\"t\":\"car\",\"seq\":5,\"cam_seq\":4,\"x\":1,\"y\":2,\"z\":3,\"qx\":0,\"qy\":0,\"qz\":0,\"qw\":1,"
		                            "\"vx\":0,\"vy\":10,\"vz\":0,\"wx\":0,\"wy\":0,\"wz\":0,\"bx\":1,\"by\":2,\"bz\":3.6,\"hx\":0.95,\"hy\":2.35,"
		                            "\"hz\":0.72,\"speed\":10,\"damage\":120.5}";
		msg::Car car;
		CHECK(car.decode(parse(fromLua)));
		CHECK(car.seq == 5 && car.cam_seq == 4 && check::near(car.vy, 10) && check::near(car.damage, 120.5));

		// Samples for the Lua side to decode (tests/e2e): one encoded message of every GTA->BeamNG type.
		if (argc > 1)
		{
			std::ofstream out(argv[1]);
			msg::Input in;
			in.seq = 3;
			in.throttle = 0.5f;
			in.steer = -0.25f;
			in.horn = true;
			msg::SpawnAt sp;
			sp.x = 10;
			sp.y = 20;
			sp.z = 30;
			sp.heading = 90;
			msg::Traffic tr;
			tr.cars.push_back({501, 18, 1, 2, 3, 0, 0, 0, 1, 0, 5, 0, 4.7f, 1.9f, 1.5f});
			msg::Mode mo;
			mo.state = "drive";
			mo.reason = "x";
			msg::Repair re;
			re.garage = "lsc_burton";
			msg::Time ti;
			ti.hour = 21;
			ti.minute = 5;
			msg::Bye by;
			by.reason = "quit";
			for (const std::string &line : {h.encode(), c.encode(), in.encode(), g.encode(), w.encode(), tr.encode(), sp.encode(), mo.encode(),
			                                re.encode(), ti.encode(), by.encode()})
				out << line << "\n";
		}
	}

	// --- Version support ---------------------------------------------------------------------------------
	{
		CHECK(version::buildFromFileVersion("1.0.3889.0") == "3889");
		CHECK(version::buildFromFileVersion("1.0.1180.2") == "1180");
		CHECK(version::buildFromFileVersion("2.0.3889.0").empty());
		CHECK(version::buildFromFileVersion("1.0.3889").empty());
		CHECK(version::buildFromFileVersion("").empty());
		CHECK(version::buildFromFileVersion("1.0.x.0").empty());
		const auto s = version::check("3889");
		CHECK(s.known);
		const auto u = version::check("9999");
		CHECK(!u.known && !u.complete);
	}

	// --- Patterns ------------------------------------------------------------------------------------------
	{
		const auto p = patterns::parse("48 8D 0D ? ? ? ? 48 8B 14 FA E8 ? ? ? ? 48 85 C0 75 0A");
		CHECK(p.valid() && p.bytes.size() == 21 && !p.mask[3] && p.mask[0]);
		CHECK(!patterns::parse("4G").valid());
		// A fake code section with the pattern at offset 100: lea rcx, [rip+0x1000]; ...; call rel32 -0x40.
		std::vector<std::uint8_t> code(400, 0xCC);
		const std::uint8_t snippet[] = {0x48, 0x8D, 0x0D, 0x00, 0x10, 0x00, 0x00, 0x48, 0x8B, 0x14, 0xFA, 0xE8, 0xC0, 0xFF, 0xFF, 0xFF, 0x48, 0x85, 0xC0, 0x75, 0x0A};
		std::memcpy(code.data() + 100, snippet, sizeof(snippet));
		const auto results = patterns::scanAll(code.data(), code.size());
		CHECK(results.size() == 1 && results[0].matches == 1);
		CHECK(patterns::lookup(results, "native_table") == code.data() + 100 + 7 + 0x1000);
		CHECK(patterns::lookup(results, "get_native_handler") == code.data() + 100 + 16 - 0x40);
		std::memcpy(code.data() + 300, snippet, sizeof(snippet));
		CHECK(patterns::count(code.data(), code.size(), p) == 2);
	}

	// --- Rotation maths --------------------------------------------------------------------------------------
	{
		const V3 f0 = fromGtaRot(0, 0, 0).rotate({0, 1, 0});
		CHECK(check::near(f0.y, 1));
		const V3 f90 = fromGtaRot(0, 0, 90).rotate({0, 1, 0}); // heading 90 faces west
		CHECK(check::near(f90.x, -1) && check::near(f90.y, 0));
		const V3 down = fromGtaRot(-10, 0, 0).rotate({0, 1, 0}); // pitch -10 looks down
		CHECK(down.z < -0.17f && down.z > -0.18f);
		const V3 rollUp = fromGtaRot(0, 30, 0).rotate({0, 0, 1}); // roll tilts the up axis sideways
		CHECK(check::near(rollUp.z, std::cos(30 * kDeg), 1e-4) && std::fabs(rollUp.x) > 0.49f);
		for (float h : {0.0f, 45.0f, 90.0f, -135.0f, 179.0f})
			CHECK(check::near(headingOf(fromHeading(h)), h, 1e-3));
		CHECK(clampf(0.0f / 0.0f, -1, 1) == 0.0f && clampf(5, 0, 1) == 1.0f);
	}

	// --- Invoker against fake handlers -----------------------------------------------------------------------
	{
		fake::reset();
		CHECK(fakeNativesMissing() == 0); // every native in the sheet has a fake
		fake::world().entities[fake::world().playerPed].pos = V3(1.5f, -2.5f, 33.0f);
		const Vector3 p = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), TRUE);
		CHECK(check::near(p.x, 1.5) && check::near(p.y, -2.5) && check::near(p.z, 33));
		Vector3 mn, mx;
		MISC::GET_MODEL_DIMENSIONS(fake::joaat("fugitive"), &mn, &mx); // vector out-params through fixVectors
		CHECK(check::near(mn.x, -0.95) && check::near(mx.y, 2.45) && check::near(mx.z, 0.9));
		alignas(8) float q[4][2] = {};
		ENTITY::GET_ENTITY_QUATERNION(PLAYER::PLAYER_PED_ID(), q[0], q[1], q[2], q[3]);
		CHECK(check::near(q[3][0], 1));
		CHECK(HUD::END_TEXT_COMMAND_THEFEED_POST_TICKER(FALSE, TRUE) == 1);
	}
	return check::summary("gta core tests");
}
