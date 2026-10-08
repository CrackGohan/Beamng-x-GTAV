-- Quaternion and frame helpers shared by the beamls modules. Plain numbers and tables only, so the maths
-- does not depend on BeamNG's vec3/quat classes (those are only used at the API boundary in gen/bngapi.lua).
-- Quaternions are {x, y, z, w}. Conventions: docs/CONTRACT.md.
local M = {}

local sqrt, sin, cos, atan2, abs = math.sqrt, math.sin, math.cos, math.atan2, math.abs
local rad, deg, tan, atan = math.rad, math.deg, math.tan, math.atan

function M.q(x, y, z, w) return {x = x, y = y, z = z, w = w} end

function M.mul(a, b)
  return {
    x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
    y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
    z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
  }
end

function M.conj(a) return {x = -a.x, y = -a.y, z = -a.z, w = a.w} end

function M.norm(a)
  local n = sqrt(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w)
  if n < 1e-12 then return {x = 0, y = 0, z = 0, w = 1} end
  return {x = a.x / n, y = a.y / n, z = a.z / n, w = a.w / n}
end

-- Rotation of angle (radians) about Z.
function M.rotZ(a) return {x = 0, y = 0, z = sin(a / 2), w = cos(a / 2)} end

-- Rotate vector v = {x,y,z} by quaternion q.
function M.rotate(q, v)
  local p = M.mul(M.mul(q, {x = v.x, y = v.y, z = v.z, w = 0}), M.conj(q))
  return {x = p.x, y = p.y, z = p.z}
end

-- GTA convention (car forward +Y) <-> BeamNG vehicle convention (car forward -Y): a half turn about Z.
local HALF_TURN = {x = 0, y = 0, z = 1, w = 0}
function M.gtaToBngVehicle(q) return M.norm(M.mul(q, HALF_TURN)) end
function M.bngVehicleToGta(q) return M.norm(M.mul(q, HALF_TURN)) end

-- GTA heading (degrees, counter-clockwise from north) -> GTA-convention quaternion.
function M.headingToQuat(headingDeg) return M.rotZ(rad(headingDeg)) end

-- Shortest-arc rotation taking +Z to the unit vector n.
function M.zTo(nx, ny, nz)
  local d = nz -- dot((0,0,1), n)
  if d > 0.999999 then return {x = 0, y = 0, z = 0, w = 1} end
  if d < -0.999999 then return {x = 1, y = 0, z = 0, w = 0} end
  -- axis = (0,0,1) x n = (-ny, nx, 0)
  return M.norm({x = -ny, y = nx, z = 0, w = 1 + d})
end

-- Rotation whose local axes are: X = right (unit, horizontal), Y = forward, Z = up (right-handed).
function M.fromAxes(rx, ry, rz, fx, fy, fz, ux, uy, uz)
  local m00, m01, m02 = rx, fx, ux
  local m10, m11, m12 = ry, fy, uy
  local m20, m21, m22 = rz, fz, uz
  local tr = m00 + m11 + m22
  local q
  if tr > 0 then
    local s = sqrt(tr + 1) * 2
    q = {w = 0.25 * s, x = (m21 - m12) / s, y = (m02 - m20) / s, z = (m10 - m01) / s}
  elseif m00 > m11 and m00 > m22 then
    local s = sqrt(1 + m00 - m11 - m22) * 2
    q = {w = (m21 - m12) / s, x = 0.25 * s, y = (m01 + m10) / s, z = (m02 + m20) / s}
  elseif m11 > m22 then
    local s = sqrt(1 + m11 - m00 - m22) * 2
    q = {w = (m02 - m20) / s, x = (m01 + m10) / s, y = 0.25 * s, z = (m12 + m21) / s}
  else
    local s = sqrt(1 + m22 - m00 - m11) * 2
    q = {w = (m10 - m01) / s, x = (m02 + m20) / s, y = (m12 + m21) / s, z = 0.25 * s}
  end
  return M.norm(q)
end

-- Angular velocity (rad/s, world frame) that turns q0 into q1 in dt seconds.
function M.angularVelocity(q0, q1, dt)
  if dt <= 0 then return 0, 0, 0 end
  local d = M.mul(q1, M.conj(q0))
  if d.w < 0 then d = {x = -d.x, y = -d.y, z = -d.z, w = -d.w} end
  local s = sqrt(d.x * d.x + d.y * d.y + d.z * d.z)
  if s < 1e-9 then return 0, 0, 0 end
  local angle = 2 * atan2(s, d.w)
  local k = angle / (s * dt)
  return d.x * k, d.y * k, d.z * k
end

-- Vertical FOV (degrees) -> horizontal FOV for an aspect ratio, and back.
function M.vfovToHfov(vfov, aspect) return deg(2 * atan(tan(rad(vfov) / 2) * aspect)) end
function M.hfovToVfov(hfov, aspect) return deg(2 * atan(tan(rad(hfov) / 2) / aspect)) end

function M.dist2(ax, ay, bx, by)
  local dx, dy = ax - bx, ay - by
  return dx * dx + dy * dy
end

M.abs = abs
return M
