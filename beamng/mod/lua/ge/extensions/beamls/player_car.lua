-- Player car: place the player's BeamNG car where GTA asks, report its pose, velocity, box and damage every
-- frame (message car) and what car it is when that changes (vehicle_info), and repair it in place.
-- (sheet systems: bng_player_car, bng_repair)
local M = {}

local ctx
local seq = 0
local prevRot, prevVeh
local infoKey
local infoTimer = 0
M.vehId = nil
M.lastInfo = nil
M.defaultModel = 'etk800'

function M.init(c)
  ctx = c
  seq = 0
  prevRot, prevVeh, infoKey = nil, nil, nil
end

function M.vehicle()
  local veh = ctx.api.get_player_vehicle()
  if veh then M.vehId = ctx.api.veh_get_id(veh) or M.vehId end
  return veh
end

-- spawn_at: GTA position and heading -> BeamNG pose (vehicle convention, forward -Y).
function M.poseFor(m)
  local o = ctx.S.origin
  local q = ctx.mathx.gtaToBngVehicle(ctx.mathx.headingToQuat(m.heading))
  return m.x - o[1], m.y - o[2], m.z - o[3], q
end

function M.spawnAt(m)
  local x, y, z, q = M.poseFor(m)
  z = z + 0.3 -- drop onto the slabs instead of spawning inside them
  local veh = M.vehicle()
  if veh then
    ctx.api.veh_set_pos_rot(veh, x, y, z, q.x, q.y, q.z, q.w)
  else
    ctx.api.spawn_vehicle(M.defaultModel, '', x, y, z, q.x, q.y, q.z, q.w)
  end
  prevRot = nil
end

-- Build the car message for this frame, or nil when there is no car.
function M.sample(dt, camSeq)
  local veh = M.vehicle()
  if not veh then return nil end
  local x, y, z = ctx.api.veh_get_position(veh)
  local qx, qy, qz, qw = ctx.api.veh_get_rotation(veh)
  if not x or not qx then return nil end
  local vx, vy, vz = ctx.api.veh_get_velocity(veh)
  vx, vy, vz = vx or 0, vy or 0, vz or 0
  local qBng = ctx.mathx.q(qx, qy, qz, qw)
  local wx, wy, wz = 0, 0, 0
  if prevRot and prevVeh == veh and dt > 0 then
    wx, wy, wz = ctx.mathx.angularVelocity(prevRot, qBng, dt)
  end
  prevRot, prevVeh = qBng, veh
  local qGta = ctx.mathx.bngVehicleToGta(qBng)
  local bx, by, bz, hx, hy, hz = ctx.api.veh_get_bbox(veh)
  if not bx then
    bx, by, bz, hx, hy, hz = x, y, z, 0.95, 2.3, 0.75 -- a mid-size car until the box is known
  end
  local o = ctx.S.origin
  seq = seq + 1
  return {
    seq = seq, cam_seq = camSeq or 0,
    x = x + o[1], y = y + o[2], z = z + o[3],
    qx = qGta.x, qy = qGta.y, qz = qGta.z, qw = qGta.w,
    vx = vx, vy = vy, vz = vz, wx = wx, wy = wy, wz = wz,
    bx = bx + o[1], by = by + o[2], bz = bz + o[3], hx = hx, hy = hy, hz = hz,
    speed = math.sqrt(vx * vx + vy * vy + vz * vz),
    damage = M.vehId and ctx.api.veh_get_damage(M.vehId) or 0,
  }
end

-- vehicle_info when the model, config or size changes (checked twice a second).
function M.info(dt, force)
  infoTimer = infoTimer - dt
  if infoTimer > 0 and not force then return nil end
  infoTimer = 0.5
  local veh = M.vehicle()
  if not veh then return nil end
  local model, config, name = ctx.api.veh_get_model(veh)
  if not model then return nil end
  local _, _, _, hx, hy, hz = ctx.api.veh_get_bbox(veh)
  local length, width, height = (hy or 2.3) * 2, (hx or 0.95) * 2, (hz or 0.75) * 2
  local key = string.format('%s|%s|%.1f|%.1f|%.1f', model, config or '', length, width, height)
  if key == infoKey and not force then return nil end
  infoKey = key
  M.lastInfo = {model = model, config = config or '', name = name or model, length = length, width = width, height = height, mass = 0}
  return M.lastInfo
end

function M.repair()
  local veh = M.vehicle()
  if not veh then return false end
  ctx.api.veh_queue_lua(veh, ctx.api.ve.ve_reset_physics())
  ctx.api.veh_reset_flexmesh(veh)
  return true
end

return M
