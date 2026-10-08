-- Camera: put BeamNG's free camera exactly where GTA's final rendered camera is, every frame, so the car is
-- drawn from GTA's point of view. (sheet systems: bng_camera)
local M = {}

local ctx
local isFree = false
M.lastSeq = 0
M.enabled = true
M.applied = 0

function M.init(c)
  ctx = c
  isFree = false
end

-- Convert a cam message to BeamNG camera arguments (position minus origin, quaternion, fov).
function M.convert(m)
  local o = ctx.S.origin
  local q = ctx.mathx.q(m.qx, m.qy, m.qz, m.qw)
  if not ctx.S.bng_cam_forward_is_y then
    -- Fallback if BeamNG's camera looks along -Y: same half turn as vehicles.
    q = ctx.mathx.gtaToBngVehicle(q)
  end
  local fov = m.fov
  if not ctx.S.bng_fov_is_vertical and m.h > 0 then
    fov = ctx.mathx.vfovToHfov(m.fov, m.w / m.h)
  end
  return m.px - o[1], m.py - o[2], m.pz - o[3], q.x, q.y, q.z, q.w, fov
end

function M.apply(m)
  if not M.enabled then return end
  if not isFree then
    isFree = ctx.api.cam_free() and true or false
  end
  local x, y, z, qx, qy, qz, qw, fov = M.convert(m)
  ctx.api.cam_set_pos_rot(x, y, z, qx, qy, qz, qw)
  ctx.api.cam_set_fov(fov)
  M.lastSeq = m.seq
  M.applied = M.applied + 1
end

-- Menus need BeamNG's own camera; give it back and retake it afterwards.
function M.setEnabled(on)
  M.enabled = on
  if on then isFree = false end
end

return M
