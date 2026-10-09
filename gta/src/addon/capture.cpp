#include "capture.hpp"
#include "../core/log.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <inspectable.h>
#include <windows.h>
#include <algorithm>
#include <cstring>

// The WinRT capture interfaces, declared here because the cross-compiler's headers leave them out. Layouts
// and IIDs follow the Windows SDK (windows.graphics.capture.h and the interop headers).
namespace wgc
{
	struct SizeInt32
	{
		INT32 Width;
		INT32 Height;
	};
	using HSTRING_ = struct HSTRING__ *;
	constexpr INT32 kPixelFormatB8G8R8A8 = 87;

	MIDL_INTERFACE("3628E81B-3CAC-4C60-B7F4-23CE0E0C3356")
	IGraphicsCaptureItemInterop : public IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE CreateForWindow(HWND window, REFIID riid, void **result) = 0;
		virtual HRESULT STDMETHODCALLTYPE CreateForMonitor(HMONITOR monitor, REFIID riid, void **result) = 0;
	};

	MIDL_INTERFACE("79C3F95B-31F7-4EC2-A464-632EF5D30760")
	IGraphicsCaptureItem : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE get_DisplayName(HSTRING_ *value) = 0;
		virtual HRESULT STDMETHODCALLTYPE get_Size(SizeInt32 *value) = 0;
		virtual HRESULT STDMETHODCALLTYPE add_Closed(void *handler, INT64 *token) = 0;
		virtual HRESULT STDMETHODCALLTYPE remove_Closed(INT64 token) = 0;
	};

	MIDL_INTERFACE("814E42A9-F70F-4AD7-939B-FDDCC6EB880D")
	IGraphicsCaptureSession_ : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE StartCapture() = 0;
	};

	MIDL_INTERFACE("2C39AE40-7D2E-5044-804E-8B6799D4CF9E")
	IGraphicsCaptureSession2_ : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE get_IsCursorCaptureEnabled(boolean *value) = 0;
		virtual HRESULT STDMETHODCALLTYPE put_IsCursorCaptureEnabled(boolean value) = 0;
	};

	MIDL_INTERFACE("F2CDD966-22AE-5EA1-9596-3A289344C3BE")
	IGraphicsCaptureSession3_ : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE get_IsBorderRequired(boolean *value) = 0;
		virtual HRESULT STDMETHODCALLTYPE put_IsBorderRequired(boolean value) = 0;
	};

	MIDL_INTERFACE("FA50C623-38DA-4B32-ACF3-FA9734AD800E")
	IDirect3D11CaptureFrame : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE get_Surface(IInspectable **value) = 0;
		virtual HRESULT STDMETHODCALLTYPE get_SystemRelativeTime(INT64 *value) = 0;
		virtual HRESULT STDMETHODCALLTYPE get_ContentSize(SizeInt32 *value) = 0;
	};

	MIDL_INTERFACE("24EB6D22-1975-422E-82E7-780DBD8DDF24")
	IDirect3D11CaptureFramePool : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE Recreate(IInspectable *device, INT32 pixelFormat, INT32 numberOfBuffers, SizeInt32 size) = 0;
		virtual HRESULT STDMETHODCALLTYPE TryGetNextFrame(IDirect3D11CaptureFrame **result) = 0;
		virtual HRESULT STDMETHODCALLTYPE add_FrameArrived(void *handler, INT64 *token) = 0;
		virtual HRESULT STDMETHODCALLTYPE remove_FrameArrived(INT64 token) = 0;
		virtual HRESULT STDMETHODCALLTYPE CreateCaptureSession(IGraphicsCaptureItem *item, IGraphicsCaptureSession_ **result) = 0;
		virtual HRESULT STDMETHODCALLTYPE get_DispatcherQueue(void **value) = 0;
	};

	MIDL_INTERFACE("589B103F-6BBC-5DF5-A991-02E28B3B66D5")
	IDirect3D11CaptureFramePoolStatics2 : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE CreateFreeThreaded(IInspectable *device, INT32 pixelFormat, INT32 numberOfBuffers, SizeInt32 size,
		                                                     IDirect3D11CaptureFramePool **result) = 0;
	};

	MIDL_INTERFACE("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")
	IDirect3DDxgiInterfaceAccess : public IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID iid, void **p) = 0;
	};

	MIDL_INTERFACE("30D5A829-7FA4-4026-83BB-D75BAE4EA99E")
	IClosable_ : public IInspectable
	{
		virtual HRESULT STDMETHODCALLTYPE Close() = 0;
	};

}

// mingw needs these for __uuidof on the interfaces above.
__CRT_UUID_DECL(wgc::IGraphicsCaptureItemInterop, 0x3628e81b, 0x3cac, 0x4c60, 0xb7, 0xf4, 0x23, 0xce, 0x0e, 0x0c, 0x33, 0x56)
__CRT_UUID_DECL(wgc::IGraphicsCaptureItem, 0x79c3f95b, 0x31f7, 0x4ec2, 0xa4, 0x64, 0x63, 0x2e, 0xf5, 0xd3, 0x07, 0x60)
__CRT_UUID_DECL(wgc::IGraphicsCaptureSession_, 0x814e42a9, 0xf70f, 0x4ad7, 0x93, 0x9b, 0xfd, 0xdc, 0xc6, 0xeb, 0x88, 0x0d)
__CRT_UUID_DECL(wgc::IGraphicsCaptureSession2_, 0x2c39ae40, 0x7d2e, 0x5044, 0x80, 0x4e, 0x8b, 0x67, 0x99, 0xd4, 0xcf, 0x9e)
__CRT_UUID_DECL(wgc::IGraphicsCaptureSession3_, 0xf2cdd966, 0x22ae, 0x5ea1, 0x95, 0x96, 0x3a, 0x28, 0x93, 0x44, 0xc3, 0xbe)
__CRT_UUID_DECL(wgc::IDirect3D11CaptureFrame, 0xfa50c623, 0x38da, 0x4b32, 0xac, 0xf3, 0xfa, 0x97, 0x34, 0xad, 0x80, 0x0e)
__CRT_UUID_DECL(wgc::IDirect3D11CaptureFramePool, 0x24eb6d22, 0x1975, 0x422e, 0x82, 0xe7, 0x78, 0x0d, 0xbd, 0x8d, 0xdf, 0x24)
__CRT_UUID_DECL(wgc::IDirect3D11CaptureFramePoolStatics2, 0x589b103f, 0x6bbc, 0x5df5, 0xa9, 0x91, 0x02, 0xe2, 0x8b, 0x3b, 0x66, 0xd5)
__CRT_UUID_DECL(wgc::IDirect3DDxgiInterfaceAccess, 0xa9b3d012, 0x3df2, 0x4ee3, 0xb8, 0xd1, 0x86, 0x95, 0xf4, 0x57, 0xd3, 0xc1)
__CRT_UUID_DECL(wgc::IClosable_, 0x30d5a829, 0x7fa4, 0x4026, 0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e)

namespace
{
	using namespace wgc;

	template <typename T>
	void release(T *&p)
	{
		if (p)
			p->Release();
		p = nullptr;
	}

	void closeAndRelease(IInspectable *&p)
	{
		if (!p)
			return;
		IClosable_ *c = nullptr;
		if (SUCCEEDED(p->QueryInterface(__uuidof(IClosable_), reinterpret_cast<void **>(&c))) && c)
		{
			c->Close();
			c->Release();
		}
		release(p);
	}

	// WinRT entry points, loaded at run time (no import library needed).
	using RoInitializeFn = HRESULT(WINAPI *)(int);
	using RoGetActivationFactoryFn = HRESULT(WINAPI *)(HSTRING_, REFIID, void **);
	using WindowsCreateStringFn = HRESULT(WINAPI *)(const wchar_t *, UINT32, HSTRING_ *);
	using WindowsDeleteStringFn = HRESULT(WINAPI *)(HSTRING_);
	using CreateD3DDeviceFn = HRESULT(WINAPI *)(IDXGIDevice *, IInspectable **);
	using DwmGetWindowAttributeFn = HRESULT(WINAPI *)(HWND, DWORD, PVOID, DWORD);

	struct State
	{
		HWND hwnd = nullptr;
		ID3D11Device *device = nullptr;
		IInspectable *rtDevice = nullptr;
		IGraphicsCaptureItem *item = nullptr;
		IDirect3D11CaptureFramePool *pool = nullptr;
		IGraphicsCaptureSession_ *session = nullptr;
		SizeInt32 poolSize{0, 0};
		ID3D11Texture2D *copy = nullptr;
		int copyW = 0, copyH = 0;
		std::uint64_t frames = 0;
		const char *problem = "not started";
	} g;

	template <typename F>
	F proc(const char *dll, const char *name)
	{
		HMODULE m = GetModuleHandleA(dll);
		if (!m)
			m = LoadLibraryA(dll);
		return m ? reinterpret_cast<F>(reinterpret_cast<void *>(GetProcAddress(m, name))) : nullptr;
	}

	template <typename T>
	bool factory(const wchar_t *cls, T **out)
	{
		auto create = proc<WindowsCreateStringFn>("combase.dll", "WindowsCreateString");
		auto destroy = proc<WindowsDeleteStringFn>("combase.dll", "WindowsDeleteString");
		auto get = proc<RoGetActivationFactoryFn>("combase.dll", "RoGetActivationFactory");
		if (!create || !destroy || !get)
			return false;
		HSTRING_ s = nullptr;
		if (FAILED(create(cls, UINT32(wcslen(cls)), &s)))
			return false;
		const HRESULT hr = get(s, __uuidof(T), reinterpret_cast<void **>(out));
		destroy(s);
		return SUCCEEDED(hr) && *out;
	}

	// Offset and size of the client area inside the captured window image (the capture includes the
	// title bar and borders of a windowed BeamNG).
	bool clientCrop(HWND hwnd, int &x, int &y, int &w, int &h)
	{
		RECT client;
		if (!GetClientRect(hwnd, &client))
			return false;
		POINT tl{0, 0};
		ClientToScreen(hwnd, &tl);
		RECT frame;
		auto dwm = proc<DwmGetWindowAttributeFn>("dwmapi.dll", "DwmGetWindowAttribute");
		constexpr DWORD kExtendedFrameBounds = 9; // DWMWA_EXTENDED_FRAME_BOUNDS
		if (!dwm || FAILED(dwm(hwnd, kExtendedFrameBounds, &frame, sizeof(frame))))
			GetWindowRect(hwnd, &frame);
		x = tl.x - frame.left;
		y = tl.y - frame.top;
		w = client.right - client.left;
		h = client.bottom - client.top;
		return w > 0 && h > 0;
	}
}

namespace beamls::capture
{
	bool running() { return g.session != nullptr; }
	void *window() { return g.hwnd; }
	const char *problem() { return g.problem; }

	void stop()
	{
		if (g.session)
		{
			IInspectable *s = g.session;
			g.session = nullptr;
			closeAndRelease(s);
		}
		if (g.pool)
		{
			IInspectable *p = g.pool;
			g.pool = nullptr;
			closeAndRelease(p);
		}
		release(g.item);
		release(g.rtDevice);
		release(g.copy);
		g.copyW = g.copyH = 0;
		g.hwnd = nullptr;
		g.device = nullptr;
	}

	bool start(ID3D11Device *device, void *hwndPtr)
	{
		HWND hwnd = static_cast<HWND>(hwndPtr);
		if (running() && hwnd == g.hwnd && device == g.device)
			return true;
		stop();
		if (!device || !hwnd || !IsWindow(hwnd))
		{
			g.problem = "no BeamNG window";
			return false;
		}
		if (auto init = proc<RoInitializeFn>("combase.dll", "RoInitialize"))
			init(1); // RO_INIT_MULTITHREADED; fails harmlessly if this thread already has COM
		IDXGIDevice *dxgi = nullptr;
		if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi))))
		{
			g.problem = "GTA's device has no DXGI device";
			return false;
		}
		auto createDevice = proc<CreateD3DDeviceFn>("d3d11.dll", "CreateDirect3D11DeviceFromDXGIDevice");
		HRESULT hr = createDevice ? createDevice(dxgi, &g.rtDevice) : E_NOINTERFACE;
		dxgi->Release();
		if (FAILED(hr))
		{
			g.problem = "Windows Graphics Capture is not available (Windows 10 1903 or newer needed)";
			return false;
		}
		IGraphicsCaptureItemInterop *interop = nullptr;
		if (!factory(L"Windows.Graphics.Capture.GraphicsCaptureItem", &interop))
		{
			g.problem = "no GraphicsCaptureItem factory";
			stop();
			return false;
		}
		hr = interop->CreateForWindow(hwnd, __uuidof(IGraphicsCaptureItem), reinterpret_cast<void **>(&g.item));
		interop->Release();
		if (FAILED(hr) || !g.item)
		{
			g.problem = "cannot capture the BeamNG window";
			stop();
			return false;
		}
		g.item->get_Size(&g.poolSize);
		IDirect3D11CaptureFramePoolStatics2 *statics = nullptr;
		if (!factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool", &statics))
		{
			g.problem = "no capture frame pool factory";
			stop();
			return false;
		}
		hr = statics->CreateFreeThreaded(g.rtDevice, kPixelFormatB8G8R8A8, 2, g.poolSize, &g.pool);
		statics->Release();
		if (FAILED(hr) || !g.pool || FAILED(g.pool->CreateCaptureSession(g.item, &g.session)) || !g.session)
		{
			g.problem = "cannot create the capture session";
			stop();
			return false;
		}
		IGraphicsCaptureSession2_ *s2 = nullptr;
		if (SUCCEEDED(g.session->QueryInterface(__uuidof(IGraphicsCaptureSession2_), reinterpret_cast<void **>(&s2))) && s2)
		{
			s2->put_IsCursorCaptureEnabled(false);
			s2->Release();
		}
		IGraphicsCaptureSession3_ *s3 = nullptr;
		if (SUCCEEDED(g.session->QueryInterface(__uuidof(IGraphicsCaptureSession3_), reinterpret_cast<void **>(&s3))) && s3)
		{
			s3->put_IsBorderRequired(false); // Windows 11: no yellow border around BeamNG
			s3->Release();
		}
		if (FAILED(g.session->StartCapture()))
		{
			g.problem = "StartCapture failed";
			stop();
			return false;
		}
		g.hwnd = hwnd;
		g.device = device;
		g.problem = "";
		log::info("capturing the BeamNG window %p (%dx%d)", static_cast<void *>(hwnd), g.poolSize.Width, g.poolSize.Height);
		return true;
	}

	bool update(ID3D11DeviceContext *ctx, Frame &out)
	{
		if (!running())
			return false;
		IDirect3D11CaptureFrame *frame = nullptr;
		bool got = false;
		// Drain to the newest frame.
		for (;;)
		{
			IDirect3D11CaptureFrame *next = nullptr;
			if (FAILED(g.pool->TryGetNextFrame(&next)) || !next)
				break;
			if (frame)
				frame->Release();
			frame = next;
		}
		if (frame)
		{
			SizeInt32 size{0, 0};
			frame->get_ContentSize(&size);
			if (size.Width != g.poolSize.Width || size.Height != g.poolSize.Height)
			{
				g.poolSize = size; // BeamNG's window was resized
				g.pool->Recreate(g.rtDevice, kPixelFormatB8G8R8A8, 2, size);
			}
			IInspectable *surface = nullptr;
			IDirect3DDxgiInterfaceAccess *access = nullptr;
			ID3D11Texture2D *src = nullptr;
			if (SUCCEEDED(frame->get_Surface(&surface)) && surface &&
			    SUCCEEDED(surface->QueryInterface(__uuidof(IDirect3DDxgiInterfaceAccess), reinterpret_cast<void **>(&access))) && access &&
			    SUCCEEDED(access->GetInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&src))) && src)
			{
				int cx = 0, cy = 0, cw = 0, ch = 0;
				if (!clientCrop(g.hwnd, cx, cy, cw, ch))
				{
					cx = cy = 0;
					cw = size.Width;
					ch = size.Height;
				}
				D3D11_TEXTURE2D_DESC sd;
				src->GetDesc(&sd);
				cw = std::min<int>(cw, int(sd.Width) - cx);
				ch = std::min<int>(ch, int(sd.Height) - cy);
				if (cw > 0 && ch > 0)
				{
					if (!g.copy || g.copyW != cw || g.copyH != ch)
					{
						release(g.copy);
						D3D11_TEXTURE2D_DESC d{};
						d.Width = UINT(cw);
						d.Height = UINT(ch);
						d.MipLevels = 1;
						d.ArraySize = 1;
						d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
						d.SampleDesc.Count = 1;
						d.Usage = D3D11_USAGE_DEFAULT;
						d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
						if (SUCCEEDED(g.device->CreateTexture2D(&d, nullptr, &g.copy)))
						{
							g.copyW = cw;
							g.copyH = ch;
						}
					}
					if (g.copy)
					{
						D3D11_BOX box{UINT(cx), UINT(cy), 0, UINT(cx + cw), UINT(cy + ch), 1};
						ctx->CopySubresourceRegion(g.copy, 0, 0, 0, 0, src, 0, &box);
						++g.frames;
						got = true;
					}
				}
			}
			if (src)
				src->Release();
			if (access)
				access->Release();
			if (surface)
				surface->Release();
			frame->Release();
		}
		out.texture = g.copy;
		out.width = g.copyW;
		out.height = g.copyH;
		out.count = g.frames;
		return got;
	}
}
