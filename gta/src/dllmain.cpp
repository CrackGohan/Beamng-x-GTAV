// BeamLS.asi entry point. Loaded by Ultimate ASI Loader (installed by Melty) into GTA5.exe.
// Start-up: log, version check, wait for GTA's native table, open the bridge, hook the script tick, register
// the compositor with ReShade. Stays off on an unsupported build or if anything is missing.
#include "addon/compositor.hpp"
#include "core/invoker.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/scripthook.hpp"
#include "core/version.hpp"
#include "game/game.hpp"
#include "gen/settings.hpp"
#include <windows.h>
#include <string>

namespace
{
	HMODULE g_module = nullptr;
	HANDLE g_thread = nullptr;
	volatile bool g_quit = false;

	bool isGta()
	{
		wchar_t path[MAX_PATH];
		GetModuleFileNameW(nullptr, path, MAX_PATH);
		const wchar_t *name = wcsrchr(path, L'\\');
		return name && _wcsicmp(name + 1, L"GTA5.exe") == 0;
	}

	DWORD WINAPI start(LPVOID)
	{
		using namespace beamls;
		const std::string dir = platform::gameDir();
		CreateDirectoryA((dir + "BeamLS").c_str(), nullptr);
		log::open(dir + settings::log_file);
		const std::string fileVersion = platform::exeVersion();
		const std::string build = version::buildFromFileVersion(fileVersion);
		const version::Support support = version::check(build);
		log::info("BeamNG x Los Santos: GTA5.exe %s (build %s)", fileVersion.c_str(), build.c_str());
		if (!support.complete)
		{
			if (!support.known)
				log::warn("GTA build %s is not supported yet: BeamLS stays off", build.c_str());
			else
				log::warn("GTA build %s: %d native hashes still missing (first: %s): BeamLS stays off", build.c_str(),
				          support.missing, support.firstMissing ? support.firstMissing : "?");
			compositor::setNotice("BeamNG x Los Santos: this GTA version is not supported yet. Update the mashup in Melty.");
			// Keep trying to register so the notice shows in ReShade's overlay.
			for (int i = 0; i < 600 && !g_quit && !compositor::tryRegister(g_module); ++i)
				Sleep(500);
			return 0;
		}

		// GTA unpacks and registers its natives during start-up: wait for the table.
		invoker::Status st;
		for (int i = 0; i < 1200 && !g_quit; ++i) // up to 10 minutes
		{
			st = invoker::init(build);
			if (st.ready && handlerFor(0x4F8644AF03D0E0D6ull /* PLAYER_ID */) != nullptr)
				break;
			Sleep(500);
		}
		if (!st.ready)
		{
			log::error("natives not available: %s", st.problem);
			compositor::setNotice("BeamNG x Los Santos could not start: see BeamLS/BeamLS.log in the GTA folder.");
			return 0;
		}
		log::info("natives ready");

		game::G().build = build;
		LARGE_INTEGER seed;
		QueryPerformanceCounter(&seed);
		game::G().bridge.start(build, static_cast<std::uint32_t>(seed.QuadPart));
		if (!scripthook::install(&game::tick))
		{
			compositor::setNotice("BeamNG x Los Santos could not hook GTA's script tick: see BeamLS/BeamLS.log.");
			return 0;
		}
		for (int i = 0; i < 600 && !g_quit && !compositor::tryRegister(g_module); ++i)
			Sleep(500);
		log::info("started");
		return 0;
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		if (!isGta())
			return TRUE; // loaded somewhere else by mistake: do nothing
		g_module = module;
		DisableThreadLibraryCalls(module);
		g_thread = CreateThread(nullptr, 0, start, nullptr, 0, nullptr);
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		g_quit = true;
		beamls::scripthook::remove();
		beamls::compositor::unregister(g_module);
		beamls::log::close();
	}
	return TRUE;
}
