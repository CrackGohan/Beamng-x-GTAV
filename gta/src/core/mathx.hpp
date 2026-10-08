// Small vector / quaternion maths for BeamLS. Conventions: docs/CONTRACT.md (Z up, metres, GTA car forward +Y).
#pragma once
#include <cmath>

namespace beamls
{
	struct V3
	{
		float x = 0, y = 0, z = 0;
		V3() = default;
		V3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
		V3 operator+(const V3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
		V3 operator-(const V3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
		V3 operator*(float s) const { return {x * s, y * s, z * s}; }
		float dot(const V3 &o) const { return x * o.x + y * o.y + z * o.z; }
		V3 cross(const V3 &o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
		float len() const { return std::sqrt(dot(*this)); }
		V3 norm() const
		{
			const float l = len();
			return l > 1e-9f ? *this * (1.0f / l) : V3(0, 0, 0);
		}
	};

	struct Quat
	{
		float x = 0, y = 0, z = 0, w = 1;
		Quat() = default;
		Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
		Quat operator*(const Quat &b) const
		{
			return {w * b.x + x * b.w + y * b.z - z * b.y,
			        w * b.y - x * b.z + y * b.w + z * b.x,
			        w * b.z + x * b.y - y * b.x + z * b.w,
			        w * b.w - x * b.x - y * b.y - z * b.z};
		}
		Quat conj() const { return {-x, -y, -z, w}; }
		Quat norm() const
		{
			const float n = std::sqrt(x * x + y * y + z * z + w * w);
			return n > 1e-12f ? Quat(x / n, y / n, z / n, w / n) : Quat();
		}
		V3 rotate(const V3 &v) const
		{
			const Quat p = (*this) * Quat(v.x, v.y, v.z, 0) * conj();
			return {p.x, p.y, p.z};
		}
		static Quat axisAngle(const V3 &axis, float rad)
		{
			const V3 a = axis.norm();
			const float s = std::sin(rad * 0.5f);
			return {a.x * s, a.y * s, a.z * s, std::cos(rad * 0.5f)};
		}
	};

	constexpr float kDeg = 3.14159265358979f / 180.0f;

	// GTA rotation order 2 (Z X Y): yaw about Z, then pitch about X, then roll about Y, degrees.
	// v_world = Rz(yaw) * Rx(pitch) * Ry(roll) * v_local. Unverified: checked by the gold-box test in game.
	inline Quat fromGtaRot(float pitchDeg, float rollDeg, float yawDeg)
	{
		const Quat qz = Quat::axisAngle({0, 0, 1}, yawDeg * kDeg);
		const Quat qx = Quat::axisAngle({1, 0, 0}, pitchDeg * kDeg);
		const Quat qy = Quat::axisAngle({0, 1, 0}, rollDeg * kDeg);
		return (qz * qx * qy).norm();
	}

	// GTA heading (degrees, counter-clockwise from north) as a quaternion.
	inline Quat fromHeading(float headingDeg) { return Quat::axisAngle({0, 0, 1}, headingDeg * kDeg); }

	// Heading (degrees) of a GTA-convention rotation's forward (+Y) axis.
	inline float headingOf(const Quat &q)
	{
		const V3 f = q.rotate({0, 1, 0});
		return std::atan2(-f.x, f.y) / kDeg;
	}

	inline float clampf(float v, float lo, float hi)
	{
		if (!(v == v))
			return lo < 0 && hi > 0 ? 0.0f : lo; // NaN
		return v < lo ? lo : (v > hi ? hi : v);
	}
}
