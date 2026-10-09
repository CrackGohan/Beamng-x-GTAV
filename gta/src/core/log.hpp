// BeamLS.log next to the game (settings.log_file). Thread-safe, line-buffered, capped in size.
#pragma once
#include <string>

namespace beamls::log
{
	void open(const std::string &path);
	void close();
	void info(const char *fmt, ...);
	void warn(const char *fmt, ...);
	void error(const char *fmt, ...);
	// The last line written (tests and the add-on overlay read it).
	std::string last();
}
