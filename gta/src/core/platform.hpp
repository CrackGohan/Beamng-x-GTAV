// Everything BeamLS needs from the operating system, behind one interface: Windows in the game
// (platform_win.cpp), a controllable fake in the Linux tests (gta/tests/platform_fake.cpp).
#pragma once
#include <string>

namespace beamls::platform
{
	double now();                 // seconds, monotonic, high resolution
	bool keyDown(int vk);         // physical key state (GetAsyncKeyState)
	bool gameHasFocus();          // GTA's window is the foreground window
	void gtaClientSize(int &w, int &h);
	std::string gameDir();        // folder of GTA5.exe, with trailing separator
	std::string exeVersion();     // GTA5.exe file version, e.g. "1.0.3889.0"; "" if unknown

	// The BeamNG window (process settings: games row beamng-drive, exe).
	void *beamngWindow();         // HWND or nullptr
	bool focusBeamNG();           // bring BeamNG to the front (GTA has focus, so Windows allows it)
	bool focusGta();              // take focus back after a BeamNG menu

	// UDP on 127.0.0.1.
	class Udp
	{
	public:
		Udp() = default;
		~Udp();
		Udp(const Udp &) = delete;
		Udp &operator=(const Udp &) = delete;
		bool open();                                   // bind 127.0.0.1:0, non-blocking
		bool sendTo(int port, const std::string &data); // to 127.0.0.1:port
		int recv(char *buf, int len, int &fromPort);    // >0 bytes, 0 nothing waiting, <0 error
		void close();
		int localPort() const { return localPort_; }

	private:
		long long sock_ = -1;
		int localPort_ = 0;
	};
}
