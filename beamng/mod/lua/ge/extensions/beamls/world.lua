-- World: GTA's ground, walls and props near the car become invisible static collision in BeamNG. Slabs are
-- TSStatic unit boxes (settings.slab_shape) that are reused: a slab no longer needed is parked far below
-- the world and handed out again later, and static collision is rebuilt in batches.
-- (sheet systems: bng_ground, bng_walls, bng_water; sheet obstacles)
local M = {}

local ctx
local active = {}   -- key -> {obj, seen, kind, born}
local free = {}     -- parked slab objects
local counts = {}   -- kind -> number active
local dirty = false
local reloadTimer = 0
local clock = 0
local water
M.stats = {created = 0, reused = 0, parked = 0, reloads = 0}

local PARK_Z = -5000
local MISSING = -9999

function M.init(c)
  ctx = c
  active, free, counts = {}, {}, {}
  dirty, reloadTimer, clock = false, 0, 0
  water = nil
  M.stats = {created = 0, reused = 0, parked = 0, reloads = 0}
end

local function place(key, kind, x, y, z, q, sx, sy, sz)
  local o = ctx.S.origin
  x, y, z = x - o[1], y - o[2], z - o[3]
  local e = active[key]
  if e then
    e.seen = clock
    if e.x and math.abs(e.x - x) < 0.02 and math.abs(e.y - y) < 0.02 and math.abs(e.z - z) < 0.02 then return e end
    if ctx.api.set_object_transform(e.obj, x, y, z, q.x, q.y, q.z, q.w, sx, sy, sz) then
      e.x, e.y, e.z = x, y, z
      dirty = true
    end
    return e
  end
  local spec = ctx.obstacles[kind]
  if (counts[kind] or 0) >= spec.maxCount then
    -- evict the oldest of this kind
    local oldKey, oldSeen
    for k, v in pairs(active) do
      if v.kind == kind and (not oldSeen or v.seen < oldSeen) then oldKey, oldSeen = k, v.seen end
    end
    if oldKey then M.release(oldKey) end
  end
  local obj = table.remove(free)
  if obj then
    if not ctx.api.set_object_transform(obj, x, y, z, q.x, q.y, q.z, q.w, sx, sy, sz) then return nil end
    M.stats.reused = M.stats.reused + 1
  else
    obj = ctx.api.create_tsstatic(ctx.S.slab_shape, x, y, z, q.x, q.y, q.z, q.w, sx, sy, sz)
    if not obj then return nil end
    M.stats.created = M.stats.created + 1
  end
  e = {obj = obj, seen = clock, kind = kind, x = x, y = y, z = z}
  active[key] = e
  counts[kind] = (counts[kind] or 0) + 1
  dirty = true
  return e
end

function M.release(key)
  local e = active[key]
  if not e then return end
  active[key] = nil
  counts[e.kind] = (counts[e.kind] or 1) - 1
  ctx.api.set_object_transform(e.obj, 0, 0, PARK_Z, 0, 0, 0, 1, 0.1, 0.1, 0.1)
  free[#free + 1] = e.obj
  M.stats.parked = M.stats.parked + 1
  dirty = true
end

-- Ground: one slab per grid cell whose top face is the best-fit plane through the cell's four corners.
function M.cellSlab(z00, z10, z01, z11, step)
  local sx = ((z10 - z00) + (z11 - z01)) / (2 * step)
  local sy = ((z01 - z00) + (z11 - z10)) / (2 * step)
  local zc = (z00 + z10 + z01 + z11) / 4
  local nl = math.sqrt(sx * sx + sy * sy + 1)
  local nx, ny, nz = -sx / nl, -sy / nl, 1 / nl
  local rl = math.sqrt(1 + sx * sx)
  local rx, ry, rz = 1 / rl, 0, sx / rl
  -- forward = up x right
  local fx, fy, fz = ny * rz - nz * ry, nz * rx - nx * rz, nx * ry - ny * rx
  local q = ctx.mathx.fromAxes(rx, ry, rz, fx, fy, fz, nx, ny, nz)
  local spec = ctx.obstacles[ctx.obstacles.ground]
  local over = 1.06
  local sizeX = step * math.sqrt(1 + sx * sx) * over
  local sizeY = step * math.sqrt(1 + sy * sy) * over
  local t = spec.thickness
  return zc, q, sizeX, sizeY, t, nx, ny, nz
end

function M.ground(m)
  local n, step = m.n, m.step
  if n < 2 or step <= 0 or #m.z < n * n then return 0 end
  local made = 0
  local function zat(i, j) return m.z[j * n + i + 1] end
  for j = 0, n - 2 do
    for i = 0, n - 2 do
      local z00, z10, z01, z11 = zat(i, j), zat(i + 1, j), zat(i, j + 1), zat(i + 1, j + 1)
      if z00 > MISSING and z10 > MISSING and z01 > MISSING and z11 > MISSING then
        local cx = m.ox + (i + 0.5) * step
        local cy = m.oy + (j + 0.5) * step
        local zc, q, sizeX, sizeY, t, nx, ny, nz = M.cellSlab(z00, z10, z01, z11, step)
        local key = string.format('g%d_%d', math.floor(cx / step), math.floor(cy / step))
        if place(key, ctx.obstacles.ground, cx - nx * t / 2, cy - ny * t / 2, zc - nz * t / 2, q, sizeX, sizeY, t) then
          made = made + 1
        end
      end
    end
  end
  return made
end

-- Walls and props: a vertical box behind each hit surface.
function M.walls(m)
  local made = 0
  for _, h in ipairs(m.hits) do
    local hl = math.sqrt(h.nx * h.nx + h.ny * h.ny)
    if hl > 0.3 then -- mostly vertical surface; flat ones are ground
      local nx, ny = h.nx / hl, h.ny / hl
      local spec = ctx.obstacles[h.kind]
      if spec and (spec.kind == 'slab' or spec.kind == 'post') then
        local width, thick = spec.width, spec.thickness
        if spec.kind == 'post' then
          width, thick = math.max(h.size * 2, 0.3), math.max(h.size * 2, 0.3)
        end
        local rx, ry = -ny, nx         -- along the wall
        local fx, fy = -nx, -ny        -- into the wall
        local q = ctx.mathx.fromAxes(rx, ry, 0, fx, fy, 0, 0, 0, 1)
        local cx, cy = h.x + fx * thick / 2, h.y + fy * thick / 2
        local cz = h.z + spec.height / 2 - 1.0
        local angle = math.floor((math.deg(math.atan2(ny, nx)) + 360) / 30 + 0.5) % 12
        local key = string.format('w%d_%d_%d_%d', h.kind, math.floor(cx / 1.0 + 0.5), math.floor(cy / 1.0 + 0.5), angle)
        if place(key, h.kind, cx, cy, cz, q, width, thick, spec.height) then made = made + 1 end
      end
    end
  end
  return made
end

function M.ensureWater()
  if water then return water end
  water = ctx.api.create_water_plane(ctx.S.ocean_z - ctx.S.origin[3])
  return water
end

function M.update(dt)
  clock = clock + dt
  -- expire what GTA has not reported for a while
  for key, e in pairs(active) do
    local spec = ctx.obstacles[e.kind]
    if spec and (clock - e.seen) * 1000 > spec.lifetimeMs then M.release(key) end
  end
  reloadTimer = reloadTimer - dt
  if dirty and reloadTimer <= 0 then
    if ctx.api.reload_static_collision() then M.stats.reloads = M.stats.reloads + 1 end
    dirty = false
    reloadTimer = ctx.S.slab_collision_reload_ms / 1000
  end
end

function M.count(kind)
  if kind then return counts[kind] or 0 end
  local n = 0
  for _ in pairs(active) do n = n + 1 end
  return n
end

function M.clear()
  for key in pairs(active) do M.release(key) end
  for _, obj in ipairs(free) do ctx.api.delete_object(obj) end
  free = {}
  if water then ctx.api.delete_object(water) water = nil end
  ctx.api.reload_static_collision()
end

return M
