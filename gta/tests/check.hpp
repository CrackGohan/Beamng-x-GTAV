// Tiny test harness: CHECK(cond) records a failure with file:line and keeps going.
#pragma once
#include <cmath>
#include <cstdio>

namespace check
{
	inline int &passed()
	{
		static int n = 0;
		return n;
	}
	inline int &failed()
	{
		static int n = 0;
		return n;
	}
	inline bool near(double a, double b, double eps = 1e-4) { return std::fabs(a - b) <= eps; }
	inline int summary(const char *suite)
	{
		std::printf("%s: %d passed, %d failed\n", suite, passed(), failed());
		return failed() == 0 ? 0 : 1;
	}
}

#define CHECK(cond)                                                              \
	do                                                                           \
	{                                                                            \
		if (cond)                                                                \
			++check::passed();                                                   \
		else                                                                     \
		{                                                                        \
			++check::failed();                                                   \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
		}                                                                        \
	} while (0)
