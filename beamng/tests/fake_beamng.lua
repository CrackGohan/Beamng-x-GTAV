-- A stand-in for BeamNG's GE Lua environment, just enough to run the beamls mod outside the game.
-- It records every call so tests can check what the mod asked BeamNG to do. It is NOT evidence that the
-- real BeamNG APIs behave like this: the bng_api sheet's unverified rows still need the game.
local json = require('json')
local F = {calls = {}, sent = {}, inbox = {}, objects = {}, vehicles = {}, veCode = {}, guihooks = {}, logs = {}}
F.root = assert(BEAMLS_ROOT, 'set BEAMLS_ROOT to the beamls extension folder')

local function rec(name, ...) F.calls[#F.calls + 1] = {name, ...} end
function F.count(name)
  local n = 0
  for _, c in ipairs(F.calls) do if c[1] == name then n = n + 1 end end
  return n
end
function F.last(name)
  for i = #F.calls, 1, -1 do if F.calls[i][1] == name then return F.calls[i] end end
end

-- logging ---------------------------------------------------------------------------------------
function log(level, tag, msg) F.logs[#F.logs + 1] = level .. ' ' .. tag .. ' ' .. tostring(msg) end
function F.logged(pattern)
  for _, l in ipairs(F.logs) do if l:find(pattern, 1, true) then return true end end
  return false
end

-- json ------------------------------------------------------------------------------------------
jsonEncode = json.encode
jsonDecode = json.decode

-- vectors ---------------------------------------------------------------------------------------
local V = {}
V.__index = V
V.__unm = function(a) return vec3(-a.x, -a.y, -a.z) end
V.__add = function(a, b) return vec3(a.x + b.x, a.y + b.y, a.z + b.z) end
V.__sub = function(a, b) return vec3(a.x - b.x, a.y - b.y, a.z - b.z) end
function vec3(x, y, z) return setmetatable({x = x or 0, y = y or 0, z = z or 0}, V) end
Point3F = vec3
function quat(x, y, z, w) return {x = x, y = y, z = z, w = w} end
local mathx = dofile(F.root .. 'mathx.lua')
function quatFromDir(dir, up)
  local function n(v) local l = math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z) return {x = v.x / l, y = v.y / l, z = v.z / l} end
  local f = n(dir)
  local u = n(up)
  local r = n({x = f.y * u.z - f.z * u.y, y = f.z * u.x - f.x * u.z, z = f.x * u.y - f.y * u.x})
  u = {x = r.y * f.z - r.z * f.y, y = r.z * f.x - r.x * f.z, z = r.x * f.y - r.y * f.x}
  return mathx.fromAxes(r.x, r.y, r.z, f.x, f.y, f.z, u.x, u.y, u.z)
end

-- socket ----------------------------------------------------------------------------------------
local fakeSock = {}
fakeSock.__index = fakeSock
function fakeSock:settimeout(t) self.timeout = t end
function fakeSock:setsockname(ip, port)
  if F.portBusy then return nil, 'address already in use' end
  self.ip, self.port = ip, port
  F.bound = {ip = ip, port = port}
  return true
end
function fakeSock:receivefrom()
  local d = table.remove(F.inbox, 1)
  if not d then return nil, 'timeout' end
  return d.data, d.ip, d.port
end
function fakeSock:sendto(data, ip, port)
  F.sent[#F.sent + 1] = {data = data, ip = ip, port = port, msg = json.decode(data)}
  return #data
end
function fakeSock:close() F.bound = nil end
if not BEAMLS_REAL_SOCKET then
  package.preload['socket'] = function() return {udp = function() return setmetatable({}, fakeSock) end} end
end

function F.deliver(t, port)
  F.inbox[#F.inbox + 1] = {data = json.encode(t), ip = '127.0.0.1', port = port or 50123}
end
function F.sentOf(t)
  local out = {}
  for _, s in ipairs(F.sent) do if s.msg and s.msg.t == t then out[#out + 1] = s.msg end end
  return out
end

-- ffi (AllowSetForegroundWindow) ------------------------------------------------------------------
package.preload['ffi'] = function()
  return {cdef = function() end, load = function()
    return {AllowSetForegroundWindow = function(pid) rec('AllowSetForegroundWindow', pid) return 1 end}
  end}
end

-- scene objects -----------------------------------------------------------------------------------
local Obj = {}
Obj.__index = Obj
function Obj:setHidden(h) self.hidden = h rec('setHidden', self.name, h) end
function Obj:setField(k, i, v) self.fields[k] = v rec('setField', self.name, k, v) end
function Obj:setPosRot(x, y, z, qx, qy, qz, qw) self.pos = {x, y, z} self.rot = {qx, qy, qz, qw} end
function Obj:setScale(s) self.scale = {s.x, s.y, s.z} end
function Obj:setPosition(p) self.pos = {p.x, p.y, p.z} end
function Obj:registerObject(name) self.registered = true end
function Obj:delete() self.deleted = true rec('delete', self.class) end
local function newObj(class, name)
  local o = setmetatable({class = class, name = name or class, fields = {}}, Obj)
  F.objects[#F.objects + 1] = o
  return o
end
function createObject(class) rec('createObject', class) return newObj(class) end
F.scene = {sunsky = newObj('ScatterSky', 'sunsky'), theGroundPlane = newObj('GroundPlane', 'theGroundPlane'), theLevelInfo = newObj('LevelInfo', 'theLevelInfo')}
scenetree = {
  findObject = function(name) return F.scene[name] end,
  MissionGroup = {addObject = function(_, o) o.inMission = true end},
}

-- vehicles ----------------------------------------------------------------------------------------
local Veh = {}
Veh.__index = Veh
local nextId = 1000
function F.newVehicle(model, config, x, y, z)
  nextId = nextId + 1
  local v = setmetatable({id = nextId, model = model, partConfig = config, pos = {x = x or 0, y = y or 0, z = z or 0},
    dir = {x = 0, y = 1, z = 0}, up = {x = 0, y = 0, z = 1}, vel = {x = 0, y = 0, z = 0}, code = {}}, Veh)
  F.vehicles[v.id] = v
  return v
end
function Veh:getID() return self.id end
function Veh:getPosition() return vec3(self.pos.x, self.pos.y, self.pos.z) end
function Veh:getDirectionVector() return vec3(self.dir.x, self.dir.y, self.dir.z) end
function Veh:getDirectionVectorUp() return vec3(self.up.x, self.up.y, self.up.z) end
function Veh:getVelocity() return vec3(self.vel.x, self.vel.y, self.vel.z) end
function Veh:getJBeamFilename() return self.model end
function Veh:getRefNodeId() return 0 end
function Veh:resetBrokenFlexMesh() rec('resetBrokenFlexMesh', self.id) end
function Veh:setClusterPosRelRot(ref, x, y, z, qx, qy, qz, qw)
  self.pos = {x = x, y = y, z = z}
  rec('setClusterPosRelRot', self.id, x, y, z, qx, qy, qz, qw)
end
function Veh:applyClusterVelocityScaleAdd(ref, s, vx, vy, vz)
  self.vel = {x = self.vel.x + vx, y = self.vel.y + vy, z = self.vel.z + vz}
  rec('applyClusterVelocityScaleAdd', self.id, vx, vy, vz)
end
function Veh:getSpawnWorldOOBB()
  local p = self.pos
  return {getCenter = function() return vec3(p.x, p.y, p.z + 0.6) end, getHalfExtents = function() return vec3(0.95, 2.35, 0.72) end}
end
function Veh:delete() self.deleted = true F.vehicles[self.id] = nil rec('deleteVehicle', self.id) end
-- Vehicle Lua: run the queued code in a sandbox that records input events.
function Veh:queueLuaCommand(code)
  self.code[#self.code + 1] = code
  F.veCode[#F.veCode + 1] = code
  local env = {
    FILTER_DIRECT = 0, RESET_PHYSICS = 1, pcall = pcall, tostring = tostring,
    log = function(l, t, m) F.logs[#F.logs + 1] = 'VE ' .. l .. ' ' .. t .. ' ' .. tostring(m) end,
    input = {
      event = function(name, value, filter, a, b, c, source) self.inputs = self.inputs or {} self.inputs[name] = {value, source} end,
      setAllowedInputSource = function(name, src, allowed) self.allowed = self.allowed or {} self.allowed[name .. '/' .. src] = allowed end,
    },
    electrics = {horn = function(on) self.horn = on end, toggle_lights = function() self.lights = not self.lights end},
    obj = {requestReset = function(mode) self.resets = (self.resets or 0) + 1 end},
  }
  local fn = assert(loadstring(code))
  setfenv(fn, env)
  fn()
end

F.player = nil
be = {
  getPlayerVehicle = function(_, i) return F.player end,
  reloadStaticCollision = function() rec('reloadStaticCollision') end,
  enterVehicle = function(_, i, v) rec('enterVehicle', v and v.id) end,
}
spawn = {safeTeleport = function(veh, pos, rot, reset)
  veh.pos = {x = pos.x, y = pos.y, z = pos.z}
  local f = mathx.rotate(mathx.q(rot.x, rot.y, rot.z, rot.w), {x = 0, y = -1, z = 0}) -- BeamNG cars face -Y
  veh.dir = {x = f.x, y = f.y, z = f.z}
  rec('safeTeleport', veh.id, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, rot.w)
end}
core_vehicles = {
  spawnNewVehicle = function(model, opts)
    local v = F.newVehicle(model, opts.config, opts.pos and opts.pos.x, opts.pos and opts.pos.y, opts.pos and opts.pos.z)
    rec('spawnNewVehicle', model, opts.config, opts.autoEnterVehicle)
    if model ~= 'beamls_proxy' then F.player = v end
    return v
  end,
  getModel = function(model) return {model = {Name = model:upper()}} end,
}
map = {objects = {}}

-- level, camera, ui, time -------------------------------------------------------------------------
F.mission = ''
function getMissionFilename() return F.mission end
core_levels = {expandMissionFileName = function(l) return '/levels/' .. l .. '/main.level.json' end}
freeroam_freeroam = {startFreeroam = function(path) rec('startFreeroam', path) F.mission = path end}
commands = {setFreeCamera = function() rec('setFreeCamera') end}
core_camera = {
  setPosRot = function(i, x, y, z, qx, qy, qz, qw) F.cam = {x, y, z, qx, qy, qz, qw} rec('camSetPosRot') end,
  setFOV = function(i, fov) F.fov = fov end,
}
guihooks = {trigger = function(name, data) F.guihooks[#F.guihooks + 1] = {name, data} rec('guihooks', name) end}
simTimeAuthority = {pause = function(p) F.simPaused = p rec('simPause', p) end}
core_environment = {setTimeOfDay = function(t) F.tod = t.time end}
beamng_versionb = '0.39.4.0'

-- extensions ----------------------------------------------------------------------------------------
F.loaded = {}
extensions = {}
local function extName(path) return (path:gsub('/', '_')) end
function extensions.load(name)
  local path = name:gsub('_', '/', 1)
  local mod = dofile(F.root .. path:gsub('^beamls/', '') .. '.lua')
  rawset(_G, extName(name), mod)
  F.loaded[extName(name)] = mod
  if mod.onExtensionLoaded then mod.onExtensionLoaded() end
  return mod
end
function extensions.unload(name)
  local mod = F.loaded[name]
  if mod and mod.onExtensionUnloaded then mod.onExtensionUnloaded() end
  F.loaded[name] = nil
  rawset(_G, name, nil)
end
function setExtensionUnloadMode(name, mode) rec('setExtensionUnloadMode', name, mode) end

-- One game frame: every loaded extension's onUpdate, like BeamNG does.
function F.frame(dt)
  dt = dt or 1 / 60
  for _, name in ipairs({'beamls_listener', 'beamls_main'}) do
    local m = F.loaded[name]
    if m and m.onUpdate then m.onUpdate(dt, dt, dt) end
  end
end

return F
