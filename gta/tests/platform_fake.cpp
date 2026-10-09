// platform.hpp for the Linux tests: real UDP on 127.0.0.1 (POSIX sockets), everything else controllable.
#include "../src/core/platform.hpp"
#include "fake_gta.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

namespace beamls::platform
{
	double now() { return fake::platform().time; }
	bool keyDown(int vk) { return fake::platform().keys.count(vk) != 0; }
	bool gameHasFocus() { return fake::platform().gtaFocused; }
	void gtaClientSize(int &w, int &h)
	{
		w = 1920;
		h = 1080;
	}
	std::string gameDir() { return "./"; }
	std::string exeVersion() { return fake::platform().exeVersion; }
	void *beamngWindow() { return fake::platform().beamngWindow; }
	bool focusBeamNG()
	{
		++fake::platform().focusBeamNGCalls;
		fake::platform().gtaFocused = false;
		return fake::platform().beamngWindow != nullptr;
	}
	bool focusGta()
	{
		++fake::platform().focusGtaCalls;
		fake::platform().gtaFocused = true;
		return true;
	}

	Udp::~Udp() { close(); }

	bool Udp::open()
	{
		const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0)
			return false;
		sockaddr_in a{};
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (::bind(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)) != 0)
		{
			::close(s);
			return false;
		}
		fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
		socklen_t len = sizeof(a);
		getsockname(s, reinterpret_cast<sockaddr *>(&a), &len);
		localPort_ = ntohs(a.sin_port);
		sock_ = s;
		return true;
	}

	bool Udp::sendTo(int port, const std::string &data)
	{
		if (sock_ < 0)
			return false;
		sockaddr_in a{};
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		a.sin_port = htons(static_cast<uint16_t>(port));
		return ::sendto(int(sock_), data.data(), data.size(), 0, reinterpret_cast<sockaddr *>(&a), sizeof(a)) == ssize_t(data.size());
	}

	int Udp::recv(char *buf, int len, int &fromPort)
	{
		if (sock_ < 0)
			return -1;
		sockaddr_in a{};
		socklen_t alen = sizeof(a);
		const ssize_t n = ::recvfrom(int(sock_), buf, size_t(len), 0, reinterpret_cast<sockaddr *>(&a), &alen);
		if (n < 0)
			return (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNREFUSED) ? 0 : -1;
		fromPort = ntohs(a.sin_port);
		return int(n);
	}

	void Udp::close()
	{
		if (sock_ >= 0)
			::close(int(sock_));
		sock_ = -1;
	}
}
