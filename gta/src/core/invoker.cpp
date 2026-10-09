// Game side of the invoker: finds GTA's native lookup function by pattern and translates original hashes
// with the build's table from sheets/natives.json (gen/xmap.hpp). Windows only.
#include "invoker.hpp"
#include "log.hpp"
#include "patterns.hpp"
#include "../gen/xmap.hpp"
#include <windows.h>
#include <mutex>
#include <unordered_map>

namespace beamls
{
	namespace
	{
		using GetNativeHandlerFn = NativeHandler (*)(void *table, std::uint64_t hash);

		void *g_table = nullptr;
		GetNativeHandlerFn g_lookup = nullptr;
		std::unordered_map<std::uint64_t, std::uint64_t> g_xmap;
		std::unordered_map<std::uint64_t, NativeHandler> g_cache;
		std::mutex g_lock;
		invoker::Status g_status;

		bool textSection(const std::uint8_t *&begin, std::size_t &size)
		{
			auto *base = reinterpret_cast<const std::uint8_t *>(GetModuleHandleW(nullptr));
			if (!base)
				return false;
			auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
			auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
			const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
			for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
			{
				if (std::memcmp(sec->Name, ".text", 5) == 0)
				{
					begin = base + sec->VirtualAddress;
					size = sec->Misc.VirtualSize;
					return true;
				}
			}
			// Some protected builds rename sections: fall back to the whole image.
			begin = base;
			size = nt->OptionalHeader.SizeOfImage;
			return true;
		}
	}

	bool nativesReady() { return g_status.ready; }

	NativeHandler handlerFor(std::uint64_t original)
	{
		if (!g_lookup || !g_table)
			return nullptr;
		std::lock_guard<std::mutex> lock(g_lock);
		auto it = g_cache.find(original);
		if (it != g_cache.end())
			return it->second;
		auto x = g_xmap.find(original);
		if (x == g_xmap.end() || x->second == 0)
			return nullptr;
		NativeHandler h = g_lookup(g_table, x->second);
		if (h)
			g_cache[original] = h;
		return h;
	}

	namespace invoker
	{
		std::uint64_t translate(std::uint64_t original)
		{
			auto x = g_xmap.find(original);
			return x == g_xmap.end() ? 0 : x->second;
		}

		Status status() { return g_status; }

		Status init(const std::string &build)
		{
			g_status = Status();
			g_xmap.clear();
			for (const auto &t : gen::kBuildTables)
			{
				if (build != t.build)
					continue;
				bool complete = true;
				for (std::size_t i = 0; i < t.count; ++i)
				{
					g_xmap[t.entries[i].original] = t.entries[i].translated;
					if (t.entries[i].translated == 0)
					{
						complete = false;
						log::warn("no %s hash for %s yet", build.c_str(), t.entries[i].name);
					}
				}
				g_status.tableComplete = complete;
			}
			if (!g_status.tableComplete)
			{
				g_status.problem = "this GTA version is not supported yet";
				return g_status;
			}
			const std::uint8_t *begin = nullptr;
			std::size_t size = 0;
			if (!textSection(begin, size))
			{
				g_status.problem = "cannot read GTA5.exe's code";
				return g_status;
			}
			const auto results = patterns::scanAll(begin, size);
			for (const auto &r : results)
			{
				log::info("pattern %s: %d match(es), first at +0x%llx", r.id.c_str(), r.matches,
				          r.match ? static_cast<unsigned long long>(r.match - reinterpret_cast<const std::uint8_t *>(GetModuleHandleW(nullptr))) : 0ull);
				for (const auto &x : r.resolved)
					log::info("  %s -> %p", x.name.c_str(), static_cast<const void *>(x.address));
			}
			g_table = const_cast<std::uint8_t *>(patterns::lookup(results, "native_table"));
			g_lookup = reinterpret_cast<GetNativeHandlerFn>(const_cast<std::uint8_t *>(patterns::lookup(results, "get_native_handler")));
			g_status.patternsFound = g_table && g_lookup;
			if (!g_status.patternsFound)
			{
				g_status.problem = "GTA's native table was not found (pattern native_lookup)";
				return g_status;
			}
			g_status.ready = true;
			g_status.problem = "";
			return g_status;
		}
	}
}
