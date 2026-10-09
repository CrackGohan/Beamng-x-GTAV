-- BeamNG side of the end-to-end test (tests/e2e.sh): the real beamls mod in LuaJIT with real LuaSocket on
-- 127.0.0.1:47011 and the fake BeamNG engine, in real time. The fake car drives from the inputs it gets
-- (simple arcade physics), so GTA's proxy can follow it. Plays the player in BeamNG's menu: picks the car
-- and presses F6 there.
local here = arg[0]:match('^(.*)/[^/]*$') or '.'
local ext = here .. '/../mod/lua/ge/extensions/'
BEAMLS_ROOT = ext .. 'beamls/'
BEAMLS_REAL_SOCKET = true
package.path = here .. '/?.lua;' .. ext .. '?.lua;/usr/share/lua/5.1/?.lua;' .. package.path
package.cpath = '/usr/lib/x86_64-linux-gnu/lua/5.1/?.so;' .. package.cpath
local socket = require('socket')
local F = require('fake_beamng')

local passed, failed = 0, 0
local function check(name, cond, detail)
  if cond then passed = passed + 1 else failed = failed + 1 print('FAIL ' .. name .. (detail and (': ' .. tostring(detail)) or '')) end
end

local modScript = assert(loadfile(here .. '/../mod/scripts/beamls/modScript.lua'))
setfenv(modScript, setmetatable({load = extensions.load}, {__index = _G}))
modScript()
check('listener bound', F.loaded.beamls_listener ~= nil and F.logged('listener on'))
F.player = F.newVehicle('etk800', 'vehicles/etk800/base.pc', 0, 0, 0)

local menuSeen, menuAt, picked = false, nil, false
local maxSpeed, sessions, ended = 0, 0, false
local t0 = socket.gettime()
local dt = 1 / 60
while socket.gettime() - t0 < 60 do
  local v = F.player
  -- arcade physics for the fake car: throttle accelerates along its nose, brake slows
  if v and v.inputs then
    local thr = v.inputs.throttle and v.inputs.throttle[1] or 0
    local brk = v.inputs.brake and v.inputs.brake[1] or 0
    local speed = math.sqrt(v.vel.x ^ 2 + v.vel.y ^ 2)
    speed = math.max(0, speed + (thr * 6 - brk * 10) * dt)
    if F.simPaused then speed = 0 end
    v.vel = {x = v.dir.x * speed, y = v.dir.y * speed, z = 0}
    v.pos = {x = v.pos.x + v.vel.x * dt, y = v.pos.y + v.vel.y * dt, z = v.pos.z}
    maxSpeed = math.max(maxSpeed, speed)
  end
  -- the player in BeamNG's vehicle selector
  for _, g in ipairs(F.guihooks) do
    if g[1] == 'ChangeState' and g[2].state == 'menu.vehicles' and not menuSeen then menuSeen, menuAt = true, socket.gettime() end
  end
  if menuSeen and not picked and socket.gettime() - menuAt > 0.5 and beamls_main then
    picked = true
    beamls_main.onKey('back_to_gta')
  end
  if F.loaded.beamls_main then sessions = 1 end
  if sessions == 1 and not F.loaded.beamls_main then ended = true break end
  F.frame(dt)
  socket.sleep(dt)
end

local slabs, proxies = 0, 0
for _, o in ipairs(F.objects) do if o.class == 'TSStatic' then slabs = slabs + 1 end end
proxies = F.count('spawnNewVehicle')
check('session started and ended cleanly on GTA bye', ended)
check('vehicle selector opened by F6 in GTA', menuSeen)
check('camera followed GTA', F.count('camSetPosRot') > 30, F.count('camSetPosRot'))
check('car placed next to the GTA player', F.count('safeTeleport') >= 1)
check('throttle from GTA moved the BeamNG car', maxSpeed > 3, maxSpeed)
check('ground and wall slabs built from GTA probes', slabs > 10, slabs)
check('police car mirrored as a proxy', proxies >= 1, proxies)
check('no bngapi failures', not F.logged('bngapi FAIL'))
print(string.format('e2e beamng side: %d passed, %d failed (max speed %.1f m/s, %d slabs, %d proxies)', passed, failed, maxSpeed, slabs, proxies))
os.exit(failed == 0 and 0 or 1)
