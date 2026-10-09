// Which GTA build is running, and whether BeamLS has a native hash table for it. (sheet systems: gta_version)
#pragma once
#include <string>

namespace beamls::version
{
	// "1.0.3889.0" -> "3889"; "" when the text is not a GTA V file version.
	std::string buildFromFileVersion(const std::string &fileVersion);

	struct Support
	{
		std::string build;      // e.g. "3889"
		bool known = false;     // a table exists for this build
		bool complete = false;  // every native the mod calls has a translated hash
		int missing = 0;        // natives without a hash
		const char *firstMissing = nullptr;
	};
	Support check(const std::string &build);
}
