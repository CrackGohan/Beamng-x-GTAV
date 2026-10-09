// Feeds BeamLS.fx each frame: BeamNG's captured picture, GTA's camera, the BeamNG car's box (now and when
// BeamNG drew the picture) and BeamNG's camera for that picture, so the shader can re-project the car onto
// GTA's current view and depth-test it against GTA's depth buffer. Adapted from universal-modder's
// Minecraft x GTA V compositor (MIT). (sheet systems: gta_compositor)
#include "compositor.hpp"
#include "capture.hpp"
#include "../core/log.hpp"
#include "../core/mathx.hpp"
#include "../core/platform.hpp"
#include "../game/composite_state.hpp"
#include "../gen/settings.hpp"
#include <d3d11.h>
#include <windows.h>
#include <reshade.hpp>
#include <atomic>
#include <cmath>
#include <string>

using namespace reshade::api;

namespace beamls::compositor
{
	namespace
	{
		constexpr const char *kEffect = "BeamLS.fx";
		std::atomic<bool> g_registered{false};
		std::string g_notice;
		resource_view g_srv = {0};
		ID3D11Texture2D *g_srvTexture = nullptr;

		struct M3
		{
			V3 r0, r1, r2; // rows; columns are the local axes in world space
		};

		M3 rows(const Quat &q)
		{
			const V3 x = q.rotate({1, 0, 0}), y = q.rotate({0, 1, 0}), z = q.rotate({0, 0, 1});
			return {{x.x, y.x, z.x}, {x.y, y.y, z.y}, {x.z, y.z, z.z}};
		}

		void setF(effect_runtime *rt, const char *name, float a)
		{
			if (auto v = rt->find_uniform_variable(kEffect, name); v.handle != 0)
				rt->set_uniform_value_float(v, a);
		}
		void setF2(effect_runtime *rt, const char *name, float a, float b)
		{
			if (auto v = rt->find_uniform_variable(kEffect, name); v.handle != 0)
				rt->set_uniform_value_float(v, a, b);
		}
		void setF3(effect_runtime *rt, const char *name, const V3 &a)
		{
			if (auto v = rt->find_uniform_variable(kEffect, name); v.handle != 0)
				rt->set_uniform_value_float(v, a.x, a.y, a.z);
		}
		void setB(effect_runtime *rt, const char *name, bool b)
		{
			if (auto v = rt->find_uniform_variable(kEffect, name); v.handle != 0)
				rt->set_uniform_value_bool(v, b);
		}
		void setM3(effect_runtime *rt, const char *prefix, const M3 &m)
		{
			const std::string p(prefix);
			setF3(rt, (p + "R0").c_str(), m.r0);
			setF3(rt, (p + "R1").c_str(), m.r1);
			setF3(rt, (p + "R2").c_str(), m.r2);
		}

		void bindTexture(effect_runtime *rt, ID3D11Texture2D *tex)
		{
			device *dev = rt->get_device();
			if (tex != g_srvTexture)
			{
				if (g_srv.handle != 0)
					dev->destroy_resource_view(g_srv);
				g_srv = {0};
				g_srvTexture = nullptr;
				if (tex && dev->create_resource_view(resource{reinterpret_cast<std::uint64_t>(tex)}, resource_usage::shader_resource,
				                                     resource_view_desc(format::b8g8r8a8_unorm), &g_srv))
					g_srvTexture = tex;
			}
			rt->update_texture_bindings("BEAMNG", g_srv, g_srv);
		}

		void onBeginEffects(effect_runtime *rt, command_list *, resource_view, resource_view)
		{
			const composite::State s = composite::read();
			// Hide the car whenever the script tick stalls (pause menu, loading): the picture would be stale.
			const bool fresh = platform::now() - s.tickTime < 0.15;
			bool active = s.draw && fresh;

			device *dev = rt->get_device();
			auto *d3d = reinterpret_cast<ID3D11Device *>(dev->get_native());
			if (s.beamngWindow && capture::window() != s.beamngWindow)
				capture::start(d3d, s.beamngWindow);
			capture::Frame frame;
			if (capture::running())
			{
				ID3D11DeviceContext *ctx = nullptr;
				d3d->GetImmediateContext(&ctx);
				capture::update(ctx, frame);
				ctx->Release();
			}
			active = active && frame.texture != nullptr;
			bindTexture(rt, frame.texture);
			setB(rt, "BeamLSActive", active);
			if (!active)
				return;

			// GTA's camera for the picture being presented (pose lag in frames, settings.gta_cam_lag_frames).
			const unsigned lag = static_cast<unsigned>(settings::gta_cam_lag_frames);
			const composite::Camera &gc = s.cams[(s.camHead + composite::kHistory - 1 - lag) % composite::kHistory];
			// The BeamNG car as drawn in the captured picture: settings.capture_lag_frames BeamNG frames old.
			const unsigned clag = static_cast<unsigned>(settings::capture_lag_frames);
			const composite::CarPose &drawn = s.cars[(s.carHead + composite::kHistory - 1 - clag) % composite::kHistory];
			const composite::CarPose *now = composite::newestCar(s);
			if (!gc.valid || !now || !drawn.valid)
			{
				setB(rt, "BeamLSActive", false);
				return;
			}
			composite::Camera bc;
			if (!composite::findCamera(s, drawn.camSeq, bc))
				bc = gc; // too old to find: no re-projection

			uint32_t bw = 0, bh = 0;
			rt->get_screenshot_width_and_height(&bw, &bh);
			const float gtaAspect = bh ? float(bw) / float(bh) : 16.0f / 9.0f;
			const float bngAspect = frame.height ? float(frame.width) / float(frame.height) : gtaAspect;

			setF3(rt, "GtaCamPos", gc.pos);
			setM3(rt, "GtaCam", rows(gc.rot));
			setF2(rt, "GtaCamTan", std::tan(gc.fov * kDeg * 0.5f), gtaAspect);
			setF(rt, "GtaNear", gc.nearClip);

			setF3(rt, "BoxCentre", now->centre);
			setM3(rt, "Box", rows(now->rot));
			setF3(rt, "BoxHalf", now->half);

			// Map a point on the car now to where that point was when BeamNG drew the picture.
			const Quat rel = (drawn.rot * now->rot.conj()).norm();
			setM3(rt, "Map", rows(rel));
			setF3(rt, "MapT", drawn.centre - rel.rotate(now->centre));

			setF3(rt, "BngCamPos", bc.pos);
			setM3(rt, "BngCam", rows(bc.rot));
			setF2(rt, "BngCamTan", std::tan(bc.fov * kDeg * 0.5f), bngAspect);

			setF3(rt, "KeyColor", V3(settings::key_color[0] / 255.0f, settings::key_color[1] / 255.0f, settings::key_color[2] / 255.0f));
			setF(rt, "KeyTolerance", settings::key_tolerance);
		}

		void onReloaded(effect_runtime *rt)
		{
			if (g_srv.handle != 0)
				rt->update_texture_bindings("BEAMNG", g_srv, g_srv);
		}

		void onDestroyRuntime(effect_runtime *rt)
		{
			if (g_srv.handle != 0)
				rt->get_device()->destroy_resource_view(g_srv);
			g_srv = {0};
			g_srvTexture = nullptr;
			capture::stop();
		}
	}

	void setNotice(const char *text)
	{
		g_notice = text;
		log::warn("%s", text);
		if (g_registered)
			reshade::log::message(reshade::log::level::warning, text);
	}

	bool tryRegister(void *module)
	{
		if (g_registered)
			return true;
		if (!reshade::register_addon(static_cast<HMODULE>(module)))
			return false;
		reshade::register_event<reshade::addon_event::reshade_begin_effects>(onBeginEffects);
		reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(onReloaded);
		reshade::register_event<reshade::addon_event::destroy_effect_runtime>(onDestroyRuntime);
		g_registered = true;
		log::info("registered with ReShade");
		if (!g_notice.empty())
			reshade::log::message(reshade::log::level::warning, g_notice.c_str());
		return true;
	}

	void unregister(void *module)
	{
		if (!g_registered)
			return;
		reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(onBeginEffects);
		reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(onReloaded);
		reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(onDestroyRuntime);
		reshade::unregister_addon(static_cast<HMODULE>(module));
		g_registered = false;
	}
}
