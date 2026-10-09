// Calling GTA natives without Script Hook V: look up the handler for a native's hash in the build being
// played and run it with a native call context laid out like the game's. (sheet systems: gta_invoker)
#pragma once
#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

namespace beamls
{
	// rage::scrNativeCallContext. Vector out-parameters are written by the handler into Buffers and copied to
	// the caller's 3 x 8-byte vectors afterwards (fixVectors), as the script VM does.
	struct NativeCtx
	{
		void *ret = nullptr;          // 0x00
		std::uint32_t argCount = 0;   // 0x08
		void *args = nullptr;         // 0x10
		std::int32_t bufferCount = 0; // 0x18
		std::uint32_t _pad = 0;       // 0x1C
		float *orig[4] = {};          // 0x20 caller's vectors (each 3 script values)
		float buffers[4][4] = {};     // 0x40 aligned temporaries
	};
	static_assert(offsetof(NativeCtx, orig) == 0x20, "layout");
	static_assert(offsetof(NativeCtx, buffers) == 0x40, "layout");

	using NativeHandler = void (*)(NativeCtx *);

	// Implemented by invoker.cpp in the game, by the fake GTA in tests.
	bool nativesReady();
	NativeHandler handlerFor(std::uint64_t originalHash);

	inline void fixVectors(NativeCtx &ctx)
	{
		while (ctx.bufferCount > 0)
		{
			--ctx.bufferCount;
			float *dst = ctx.orig[ctx.bufferCount];
			const float *src = ctx.buffers[ctx.bufferCount];
			if (!dst)
				continue;
			// each component sits in its own 8-byte script value
			dst[0] = src[0];
			dst[2] = src[1];
			dst[4] = src[2];
		}
	}

	template <typename R, typename... A>
	R invoke(std::uint64_t originalHash, A... args)
	{
		static_assert(sizeof...(A) <= 16, "too many native arguments");
		alignas(16) std::uint64_t argBuf[16] = {};
		alignas(16) std::uint64_t retBuf[4] = {};
		NativeCtx ctx;
		ctx.ret = retBuf;
		ctx.args = argBuf;
		std::uint32_t i = 0;
		[[maybe_unused]] auto push = [&](auto v) {
			static_assert(sizeof(v) <= 8, "native arguments are at most 8 bytes");
			std::memcpy(&argBuf[i++], &v, sizeof(v));
		};
		(push(args), ...);
		ctx.argCount = i;
		NativeHandler h = handlerFor(originalHash);
		if (!h)
		{
			if constexpr (std::is_void_v<R>)
				return;
			else
				return R{};
		}
		h(&ctx);
		fixVectors(ctx);
		if constexpr (!std::is_void_v<R>)
		{
			R r;
			std::memcpy(&r, retBuf, sizeof(R));
			return r;
		}
	}

	namespace invoker
	{
		// Game side only (invoker.cpp): find the native table for the build, check its hash table.
		struct Status
		{
			bool patternsFound = false;
			bool tableComplete = false;
			bool ready = false;
			const char *problem = "not started";
		};
		Status init(const std::string &build);
		Status status();
		std::uint64_t translate(std::uint64_t originalHash); // 0 if the build's table has no entry
	}
}
