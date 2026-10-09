#include "scripthook.hpp"
#include "invoker.hpp"
#include "log.hpp"
#include "platform.hpp"
#include "../gen/natives.hpp"
#include "../gen/settings.hpp"
#include "../gen/xmap.hpp"
#include <MinHook.h>
#include <atomic>
#include <cstring>

namespace beamls::scripthook
{
	namespace
	{
		TickFn g_tick = nullptr;
		NativeHandler g_target = nullptr;
		NativeHandler g_original = nullptr;
		bool g_inTick = false;
		int g_lastFrame = -1;
		std::atomic<unsigned long long> g_ticks{0};
		std::atomic<double> g_lastTime{0.0};

		std::uint64_t tickNativeHash()
		{
			for (const auto &t : gen::kBuildTables)
				for (std::size_t i = 0; i < t.count; ++i)
					if (std::strcmp(t.entries[i].name, settings::tick_native) == 0)
						return t.entries[i].original;
			return 0;
		}

		void detour(NativeCtx *ctx)
		{
			g_original(ctx);
			if (g_inTick || !g_tick)
				return;
			g_inTick = true;
			// Many scripts call this native every frame: tick only on the first call of each frame.
			const int frame = MISC::GET_FRAME_COUNT();
			if (frame != g_lastFrame)
			{
				g_lastFrame = frame;
				g_tick();
				++g_ticks;
				g_lastTime = platform::now();
			}
			g_inTick = false;
		}
	}

	bool install(TickFn tick)
	{
		g_tick = tick;
		const std::uint64_t hash = tickNativeHash();
		g_target = hash ? handlerFor(hash) : nullptr;
		if (!g_target)
		{
			log::error("tick native %s has no handler", settings::tick_native);
			return false;
		}
		if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
		{
			log::error("MinHook init failed");
			return false;
		}
		if (MH_CreateHook(reinterpret_cast<void *>(g_target), reinterpret_cast<void *>(&detour),
		                  reinterpret_cast<void **>(&g_original)) != MH_OK ||
		    MH_EnableHook(reinterpret_cast<void *>(g_target)) != MH_OK)
		{
			log::error("could not hook the %s handler", settings::tick_native);
			return false;
		}
		log::info("ticking from the %s handler at %p", settings::tick_native, reinterpret_cast<void *>(g_target));
		return true;
	}

	void remove()
	{
		if (g_target)
		{
			MH_DisableHook(reinterpret_cast<void *>(g_target));
			MH_RemoveHook(reinterpret_cast<void *>(g_target));
		}
		g_target = nullptr;
		g_tick = nullptr;
	}

	bool installed() { return g_target != nullptr; }
	unsigned long long ticks() { return g_ticks; }
	double lastTickTime() { return g_lastTime; }
}
