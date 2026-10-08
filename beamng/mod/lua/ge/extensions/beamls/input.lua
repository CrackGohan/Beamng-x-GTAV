-- Input: apply GTA's controls to the player's BeamNG car through the vehicle VM's input system, and park
-- the car when nobody drives it. (sheet systems: bng_input)
local M = {}

local ctx
local takenOver = {} -- vehicle object -> true once BeamNG's own input for our controls is disabled
local lastHorn = false
M.sent = 0

local NAMES = {'throttle', 'brake', 'steering', 'parkingbrake'}

function M.init(c)
  ctx = c
  takenOver = {}
  lastHorn = false
end

local function takeOver(veh)
  if takenOver[veh] then return end
  local parts = {}
  for _, n in ipairs(NAMES) do parts[#parts + 1] = ctx.api.ve.ve_input_allowed_source(n, false) end
  ctx.api.veh_queue_lua(veh, table.concat(parts, '\n'))
  takenOver[veh] = true
end

local function clamp(v, lo, hi)
  if v ~= v then return 0 end -- NaN
  if v < lo then return lo end
  if v > hi then return hi end
  return v
end

-- One vehicle-VM chunk per frame with all control values.
function M.code(m)
  local parts = {
    ctx.api.ve.ve_input_event('throttle', clamp(m.throttle, 0, 1)),
    ctx.api.ve.ve_input_event('brake', clamp(m.brake, 0, 1)),
    ctx.api.ve.ve_input_event('steering', clamp(m.steer, -1, 1)),
    ctx.api.ve.ve_input_event('parkingbrake', clamp(m.handbrake, 0, 1)),
  }
  if m.horn ~= lastHorn then
    parts[#parts + 1] = ctx.api.ve.ve_horn(m.horn)
    lastHorn = m.horn
  end
  if m.lights then parts[#parts + 1] = ctx.api.ve.ve_lights() end
  return table.concat(parts, '\n')
end

function M.apply(m)
  local veh = ctx.car.vehicle()
  if not veh then return end
  takeOver(veh)
  ctx.api.veh_queue_lua(veh, M.code(m))
  M.sent = M.sent + 1
end

-- Nobody in the car: no throttle, full parking brake.
function M.park()
  M.apply({throttle = 0, brake = 0, steer = 0, handbrake = 1, horn = false, lights = false})
end

-- Give BeamNG its own controls back (session end).
function M.release()
  for veh in pairs(takenOver) do
    local parts = {}
    for _, n in ipairs(NAMES) do parts[#parts + 1] = ctx.api.ve.ve_input_allowed_source(n, true) end
    pcall(ctx.api.veh_queue_lua, veh, table.concat(parts, '\n'))
  end
  takenOver = {}
end

return M
