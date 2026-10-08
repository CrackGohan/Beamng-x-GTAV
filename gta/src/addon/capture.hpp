// Windows Graphics Capture of BeamNG's window straight onto GTA's D3D11 device. The newest frame is copied
// (client area only) into a texture the compositor samples. (sheet systems: gta_capture)
#pragma once
#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

namespace beamls::capture
{
	struct Frame
	{
		ID3D11Texture2D *texture = nullptr; // owned by the capture; valid until the next update
		int width = 0, height = 0;          // client-area size
		std::uint64_t count = 0;            // frames copied so far
	};

	// Start (or restart, when the window changes) capturing hwnd with GTA's device.
	bool start(ID3D11Device *device, void *hwnd);
	// Copy the newest captured frame, if any, into the capture texture. Render thread only.
	bool update(ID3D11DeviceContext *ctx, Frame &out);
	void stop();
	bool running();
	void *window();
	const char *problem();
}
