// Windows implementation of platform.hpp, including window focus between GTA and BeamNG.
// (sheet systems: gta_focus, gta_bridge)
#include "platform.hpp"
#include "../gen/settings.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#include <cstring>
#include <string>
#include <vector>

namespace beamls::platform
{
	namespace
	{
		const wchar_t *kBeamNGExe = L"BeamNG.drive.x64.exe";

		struct FindCtx
		{
			DWORD pid = 0;
			bool wantOwn = false;
			HWND best = nullptr;
			long bestArea = 0;
		};

		std::wstring processName(DWORD pid)
		{
			std::wstring out;
			HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
			if (!h)
				return out;
			wchar_t buf[MAX_PATH];
			DWORD n = MAX_PATH;
			if (QueryFullProcessImageNameW(h, 0, buf, &n))
			{
				out.assign(buf, n);
				const auto slash = out.find_last_of(L"\\/");
				if (slash != std::wstring::npos)
					out = out.substr(slash + 1);
			}
			CloseHandle(h);
			return out;
		}

		BOOL CALLBACK enumProc(HWND hwnd, LPARAM lp)
		{
			auto *ctx = reinterpret_cast<FindCtx *>(lp);
			if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr)
				return TRUE;
			DWORD pid = 0;
			GetWindowThreadProcessId(hwnd, &pid);
			if (ctx->wantOwn)
			{
				if (pid != GetCurrentProcessId())
					return TRUE;
			}
			else if (_wcsicmp(processName(pid).c_str(), kBeamNGExe) != 0)
				return TRUE;
			RECT r;
			GetClientRect(hwnd, &r);
			const long area = (r.right - r.left) * (r.bottom - r.top);
			if (area > ctx->bestArea)
			{
				ctx->best = hwnd;
				ctx->bestArea = area;
			}
			return TRUE;
		}

		HWND ownWindow()
		{
			static HWND cached = nullptr;
			if (cached && IsWindow(cached))
				return cached;
			FindCtx ctx;
			ctx.wantOwn = true;
			EnumWindows(enumProc, reinterpret_cast<LPARAM>(&ctx));
			cached = ctx.best;
			return cached;
		}

		bool wsaReady()
		{
			static bool ok = [] {
				WSADATA d;
				return WSAStartup(MAKEWORD(2, 2), &d) == 0;
			}();
			return ok;
		}
	}

	double now()
	{
		static LARGE_INTEGER freq = [] {
			LARGE_INTEGER f;
			QueryPerformanceFrequency(&f);
			return f;
		}();
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		return double(t.QuadPart) / double(freq.QuadPart);
	}

	bool keyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

	bool gameHasFocus()
	{
		HWND fg = GetForegroundWindow();
		DWORD pid = 0;
		GetWindowThreadProcessId(fg, &pid);
		return pid == GetCurrentProcessId();
	}

	void gtaClientSize(int &w, int &h)
	{
		HWND hwnd = ownWindow();
		RECT r{0, 0, 1920, 1080};
		if (hwnd)
			GetClientRect(hwnd, &r);
		w = r.right - r.left;
		h = r.bottom - r.top;
		if (w <= 0 || h <= 0)
		{
			w = 1920;
			h = 1080;
		}
	}

	std::string gameDir()
	{
		char buf[MAX_PATH];
		const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
		std::string p(buf, n);
		const auto slash = p.find_last_of("\\/");
		return slash == std::string::npos ? std::string() : p.substr(0, slash + 1);
	}

	std::string exeVersion()
	{
		wchar_t path[MAX_PATH];
		GetModuleFileNameW(nullptr, path, MAX_PATH);
		DWORD dummy = 0;
		const DWORD size = GetFileVersionInfoSizeW(path, &dummy);
		if (size == 0)
			return "";
		std::vector<char> data(size);
		if (!GetFileVersionInfoW(path, 0, size, data.data()))
			return "";
		VS_FIXEDFILEINFO *info = nullptr;
		UINT len = 0;
		if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&info), &len) || !info)
			return "";
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
		              HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
		return buf;
	}

	void *beamngWindow()
	{
		static HWND cached = nullptr;
		static double lastSearch = -1e9;
		if (cached && IsWindow(cached))
			return cached;
		if (now() - lastSearch < 2.0)
			return nullptr;
		lastSearch = now();
		FindCtx ctx;
		EnumWindows(enumProc, reinterpret_cast<LPARAM>(&ctx));
		cached = ctx.best;
		return cached;
	}

	bool focusBeamNG()
	{
		HWND b = static_cast<HWND>(beamngWindow());
		if (!b)
			return false;
		if (IsIconic(b))
			ShowWindow(b, SW_RESTORE);
		return SetForegroundWindow(b) != FALSE;
	}

	bool focusGta()
	{
		HWND own = ownWindow();
		if (!own)
			return false;
		if (SetForegroundWindow(own))
			return true;
		// Windows refuses focus changes from background processes; attach to the foreground thread's input
		// queue for the call, which it allows.
		HWND fg = GetForegroundWindow();
		const DWORD fgThread = GetWindowThreadProcessId(fg, nullptr);
		const DWORD ourThread = GetCurrentThreadId();
		bool ok = false;
		if (fgThread && AttachThreadInput(ourThread, fgThread, TRUE))
		{
			ok = SetForegroundWindow(own) != FALSE;
			AttachThreadInput(ourThread, fgThread, FALSE);
		}
		return ok;
	}

	Udp::~Udp() { close(); }

	bool Udp::open()
	{
		if (!wsaReady())
			return false;
		SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (s == INVALID_SOCKET)
			return false;
		sockaddr_in a{};
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		a.sin_port = 0;
		u_long nb = 1;
		if (bind(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)) != 0 || ioctlsocket(s, FIONBIO, &nb) != 0)
		{
			closesocket(s);
			return false;
		}
		// Without this, a datagram to a closed port (BeamNG not up yet) makes later receives fail.
		BOOL reportReset = FALSE;
		DWORD bytes = 0;
		WSAIoctl(s, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &bytes, nullptr, nullptr);
		int len = sizeof(a);
		getsockname(s, reinterpret_cast<sockaddr *>(&a), &len);
		localPort_ = ntohs(a.sin_port);
		sock_ = static_cast<long long>(s);
		return true;
	}

	bool Udp::sendTo(int port, const std::string &data)
	{
		if (sock_ < 0)
			return false;
		sockaddr_in a{};
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		a.sin_port = htons(static_cast<u_short>(port));
		return sendto(static_cast<SOCKET>(sock_), data.data(), int(data.size()), 0, reinterpret_cast<sockaddr *>(&a), sizeof(a)) ==
		       int(data.size());
	}

	int Udp::recv(char *buf, int len, int &fromPort)
	{
		if (sock_ < 0)
			return -1;
		sockaddr_in a{};
		int alen = sizeof(a);
		const int n = recvfrom(static_cast<SOCKET>(sock_), buf, len, 0, reinterpret_cast<sockaddr *>(&a), &alen);
		if (n == SOCKET_ERROR)
		{
			const int e = WSAGetLastError();
			// WSAECONNRESET: an earlier datagram hit a closed port (BeamNG not running yet); keep going.
			return (e == WSAEWOULDBLOCK || e == WSAECONNRESET) ? 0 : -1;
		}
		fromPort = ntohs(a.sin_port);
		return n;
	}

	void Udp::close()
	{
		if (sock_ >= 0)
			closesocket(static_cast<SOCKET>(sock_));
		sock_ = -1;
	}
}
