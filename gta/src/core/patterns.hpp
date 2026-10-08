// IDA-style byte pattern search over a memory range, and RIP-relative address resolution.
// (sheet systems: gta_patterns; sheet patterns)
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace beamls::patterns
{
	struct Pattern
	{
		std::vector<std::uint8_t> bytes;
		std::vector<bool> mask; // true = must match
		bool valid() const { return !bytes.empty() && bytes.size() == mask.size(); }
	};

	Pattern parse(const char *ida);
	// First match in [begin, begin + size), or nullptr.
	const std::uint8_t *find(const std::uint8_t *begin, std::size_t size, const Pattern &p);
	// Number of matches (a good pattern has exactly one).
	int count(const std::uint8_t *begin, std::size_t size, const Pattern &p);
	// Target of the rel32 at match + offset: match + offset + 4 + rel32.
	const std::uint8_t *rip(const std::uint8_t *match, int offset);

	struct Resolved
	{
		std::string name;
		const std::uint8_t *address = nullptr;
	};
	struct Result
	{
		std::string id;
		const std::uint8_t *match = nullptr;
		int matches = 0;
		std::vector<Resolved> resolved;
	};
	// Run every row of sheet patterns over the range.
	std::vector<Result> scanAll(const std::uint8_t *begin, std::size_t size);
	const std::uint8_t *lookup(const std::vector<Result> &results, const char *name);
}
