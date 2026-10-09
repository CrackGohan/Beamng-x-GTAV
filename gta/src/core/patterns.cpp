#include "patterns.hpp"
#include "../gen/patterns.hpp"
#include <cstdlib>
#include <cstring>

namespace beamls::patterns
{
	Pattern parse(const char *ida)
	{
		Pattern p;
		const char *s = ida;
		while (*s)
		{
			while (*s == ' ')
				++s;
			if (!*s)
				break;
			if (*s == '?')
			{
				p.bytes.push_back(0);
				p.mask.push_back(false);
				while (*s == '?')
					++s;
			}
			else
			{
				char hex[3] = {s[0], s[1] ? s[1] : '\0', 0};
				char *end = nullptr;
				const long v = std::strtol(hex, &end, 16);
				if (end != hex + 2)
					return Pattern();
				p.bytes.push_back(static_cast<std::uint8_t>(v));
				p.mask.push_back(true);
				s += 2;
			}
		}
		return p;
	}

	const std::uint8_t *find(const std::uint8_t *begin, std::size_t size, const Pattern &p)
	{
		if (!p.valid() || size < p.bytes.size())
			return nullptr;
		const std::size_t n = p.bytes.size();
		for (std::size_t i = 0; i + n <= size; ++i)
		{
			std::size_t j = 0;
			for (; j < n; ++j)
				if (p.mask[j] && begin[i + j] != p.bytes[j])
					break;
			if (j == n)
				return begin + i;
		}
		return nullptr;
	}

	int count(const std::uint8_t *begin, std::size_t size, const Pattern &p)
	{
		int c = 0;
		const std::uint8_t *at = begin;
		std::size_t left = size;
		while (const std::uint8_t *m = find(at, left, p))
		{
			++c;
			const std::size_t used = std::size_t(m - at) + 1;
			at += used;
			left -= used;
		}
		return c;
	}

	const std::uint8_t *rip(const std::uint8_t *match, int offset)
	{
		std::int32_t rel;
		std::memcpy(&rel, match + offset, 4);
		return match + offset + 4 + rel;
	}

	std::vector<Result> scanAll(const std::uint8_t *begin, std::size_t size)
	{
		std::vector<Result> out;
		for (const auto &row : gen::kPatterns)
		{
			Result r;
			r.id = row.id;
			const Pattern p = parse(row.bytes);
			r.match = find(begin, size, p);
			r.matches = r.match ? count(begin, size, p) : 0;
			if (r.match)
				for (int i = 0; i < row.resolveCount; ++i)
					r.resolved.push_back({row.resolves[i].name, rip(r.match, row.resolves[i].offset)});
			out.push_back(r);
		}
		return out;
	}

	const std::uint8_t *lookup(const std::vector<Result> &results, const char *name)
	{
		for (const auto &r : results)
			for (const auto &x : r.resolved)
				if (x.name == name)
					return x.address;
		return nullptr;
	}
}
