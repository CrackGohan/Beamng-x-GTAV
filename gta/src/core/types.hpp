// Script-facing GTA V types, laid out the way the script VM passes them to native handlers.
#pragma once
#include <cstdint>

using Void = std::uint64_t;
using Any = std::uint64_t;
using Hash = std::uint32_t;
using Entity = int;
using Player = int;
using Ped = int;
using Vehicle = int;
using Object = int;
using Cam = int;
#ifndef _WINDEF_
using BOOL = int;
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// A script vector: three floats, each in its own 8-byte script value.
#pragma pack(push, 1)
struct Vector3
{
	float x = 0.0f;
	std::uint32_t _px = 0;
	float y = 0.0f;
	std::uint32_t _py = 0;
	float z = 0.0f;
	std::uint32_t _pz = 0;
};
#pragma pack(pop)
static_assert(sizeof(Vector3) == 24, "script vectors are 3 x 8 bytes");

inline Vector3 v3(float x, float y, float z)
{
	Vector3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}
