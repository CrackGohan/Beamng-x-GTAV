#include "log.hpp"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace beamls::log
{
	namespace
	{
		std::mutex g_lock;
		std::FILE *g_file = nullptr;
		std::string g_last;
		long g_written = 0;
		constexpr long kMaxBytes = 8 * 1024 * 1024;

		void write(const char *level, const char *fmt, va_list ap)
		{
			char msg[1024];
			std::vsnprintf(msg, sizeof(msg), fmt, ap);
			std::lock_guard<std::mutex> lock(g_lock);
			g_last = msg;
			if (!g_file || g_written > kMaxBytes)
				return;
			std::time_t t = std::time(nullptr);
			char stamp[32];
			std::strftime(stamp, sizeof(stamp), "%H:%M:%S", std::localtime(&t));
			g_written += std::fprintf(g_file, "%s %s %s\n", stamp, level, msg);
			std::fflush(g_file);
		}
	}

	void open(const std::string &path)
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (g_file)
			std::fclose(g_file);
		g_file = std::fopen(path.c_str(), "w");
		g_written = 0;
	}

	void close()
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (g_file)
			std::fclose(g_file);
		g_file = nullptr;
	}

	void info(const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		write("I", fmt, ap);
		va_end(ap);
	}

	void warn(const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		write("W", fmt, ap);
		va_end(ap);
	}

	void error(const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		write("E", fmt, ap);
		va_end(ap);
	}

	std::string last()
	{
		std::lock_guard<std::mutex> lock(g_lock);
		return g_last;
	}
}
