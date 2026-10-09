-- Traffic: every GTA vehicle near the car (traffic, police) gets an invisible solid BeamNG proxy (the
-- beamls_proxy jbeam: a rigid box with the GTA car's size class and mass) that follows GTA's pose and
-- velocity, so rams and crashes deform the player's BeamNG car for real.
-- (sheet systems: bng_traffic; sheets traffic_proxies, obstacles row 3)
local M = {}

local ctx
local proxies = {} -- GTA entity id -> {veh, ref, cfg, target, seen}
local clock = 0
M.stats = {spawned = 0, removed = 0}

function M.init(c)
  ctx = c
  proxies = {}
  clock = 0
  M.stats = {spawned = 0, removed = 0}
end

function M.pickConfig(length, height)
  for _, row in ipairs(ctx.traffic_proxies) do
    if length <= row.maxLength and height <= row.maxHeight then return row end
  end
  return ctx.traffic_proxies[#ctx.traffic_proxies]
end

local function remove(id)
  local p = proxies[id]
  if not p then return end
  proxies[id] = nil
  if p.veh then ctx.api.delete_object(p.veh) end
  M.stats.removed = M.stats.removed + 1
end

local function count()
  local n = 0
  for _ in pairs(proxies) do n = n + 1 end
  return n
end

function M.update(m)
  for _, car in ipairs(m.cars) do
    local p = proxies[car.id]
    local cfg = M.pickConfig(car.l, car.h)
    if p and p.cfg ~= cfg then remove(car.id) p = nil end -- GTA reused the handle for another vehicle
    if not p and count() < ctx.S.traffic_max then
      local o = ctx.S.origin
      local q = ctx.mathx.gtaToBngVehicle(ctx.mathx.q(car.qx, car.qy, car.qz, car.qw))
      local player = ctx.api.get_player_vehicle()
      local veh = ctx.api.spawn_vehicle('beamls_proxy', cfg.config, car.x - o[1], car.y - o[2], car.z - o[3], q.x, q.y, q.z, q.w)
      if veh then
        if player then ctx.api.enter_vehicle(player) end
        p = {veh = veh, ref = ctx.api.veh_get_ref_node(veh), cfg = cfg}
        proxies[car.id] = p
        M.stats.spawned = M.stats.spawned + 1
      end
    end
    if p then
      p.target = car
      p.seen = clock
      p.age = 0
    end
  end
end

-- Steer each proxy onto GTA's pose (extrapolated by the time since the message) every frame.
function M.follow(dt)
  clock = clock + dt
  local lifetime = ctx.obstacles[ctx.obstacles.vehicle].lifetimeMs / 1000
  for id, p in pairs(proxies) do
    if clock - p.seen > lifetime then
      remove(id)
    elseif p.target and p.ref then
      p.age = (p.age or 0) + dt
      local t, o = p.target, ctx.S.origin
      local tx = t.x + t.vx * p.age - o[1]
      local ty = t.y + t.vy * p.age - o[2]
      local tz = t.z + t.vz * p.age - o[3]
      local qT = ctx.mathx.gtaToBngVehicle(ctx.mathx.q(t.qx, t.qy, t.qz, t.qw))
      local cx, cy, cz, cw = ctx.api.veh_get_rotation(p.veh)
      if cx then
        local cur = ctx.mathx.q(cx, cy, cz, cw)
        local rel = ctx.mathx.norm(ctx.mathx.mul(ctx.mathx.conj(cur), qT))
        ctx.api.veh_set_cluster_pos_rot(p.veh, p.ref, tx, ty, tz, rel.x, rel.y, rel.z, rel.w)
        -- setClusterPosRelRot also turns the current velocity; add what is missing to reach GTA's
        local vx, vy, vz = ctx.api.veh_get_velocity(p.veh)
        if vx then
          local v = ctx.mathx.rotate(rel, {x = vx, y = vy, z = vz})
          ctx.api.veh_apply_cluster_velocity(p.veh, p.ref, t.vx - v.x, t.vy - v.y, t.vz - v.z)
        end
      end
    end
  end
end

function M.count() return count() end

function M.clear()
  for id in pairs(proxies) do remove(id) end
end

return M
