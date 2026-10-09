#include "version.hpp"
#include "../gen/xmap.hpp"
#include <cctype>
#include <cstring>
#include <vector>

namespace beamls::version
{
	std::string buildFromFileVersion(const std::string &v)
	{
		// Expect four dot-separated numbers: 1.0.<build>.<revision>
		std::vector<std::string> parts;
		std::string cur;
		for (char c : v)
		{
			if (c == '.')
			{
				parts.push_back(cur);
				cur.clear();
			}
			else if (std::isdigit(static_cast<unsigned char>(c)))
				cur += c;
			else
				return "";
		}
		parts.push_back(cur);
		if (parts.size() != 4 || parts[0] != "1" || parts[1] != "0" || parts[2].empty())
			return "";
		return parts[2];
	}

	Support check(const std::string &build)
	{
		Support s;
		s.build = build;
		for (const auto &t : gen::kBuildTables)
		{
			if (build != t.build)
				continue;
			s.known = true;
			for (std::size_t i = 0; i < t.count; ++i)
			{
				if (t.entries[i].translated == 0)
				{
					if (!s.firstMissing)
						s.firstMissing = t.entries[i].name;
					++s.missing;
				}
			}
			s.complete = s.missing == 0;
		}
		return s;
	}
}
