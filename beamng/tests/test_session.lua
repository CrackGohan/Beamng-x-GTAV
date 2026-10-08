-- Plays a whole mashup session against fake_beamng.lua: hello, level load, camera, spawn, driving input,
-- ground and walls, traffic, menus, repair, bye, protocol mismatch and timeout.
--   luajit beamng/tests/test_session.lua      (from the repo root)
local here = arg[0]:match('^(.*)/[^/]*$') or '.'
local ext = here .. '/../mod/lua/ge/extensions/'
BEAMLS_ROOT = ext .. 'beamls/'
package.path = here .. '/?.lua;' .. ext .. '?.lua;' .. package.path

local F = require('fake_beamng')
local mathx = dofile(BEAMLS_ROOT .. 'mathx.lua')
local S = dofile(BEAMLS_ROOT .. 'gen/settings.lua')

local passed, failed = 0, 0
local function check(name, cond, detail)
  if cond then passed = passed + 1 else failed = failed + 1 print('FAIL ' .. name .. (detail and (': ' .. tostring(detail)) or '')) end
end
local function near(a, b, eps) return math.abs(a - b) <= (eps or 1e-4) end
local function frames(n, dt) for _ = 1, n do F.frame(dt) end end
local function sameRot(a, b)
  -- q and -q are the same rotation
  local d = math.abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w)
  return near(d, 1, 1e-4)
end

-- 1. modScript loads only the listener -------------------------------------------------------------
local modScript = assert(loadfile(here .. '/../mod/scripts/beamls/modScript.lua'))
setfenv(modScript, setmetatable({load = extensions.load}, {__index = _G}))
modScript()
check('listener loaded', F.loaded.beamls_listener ~= nil)
check('listener bound to bridge port', F.bound and F.bound.port == S.bridge_port and F.bound.ip == '127.0.0.1')
check('main not loaded before hello', F.loaded.beamls_main == nil)
frames(30)
check('idle listener sends nothing', #F.sent == 0)
check('idle listener loads no level', F.count('startFreeroam') == 0)

-- 2. hello starts the session ------------------------------------------------------------------------
F.player = F.newVehicle('etk800', 'vehicles/etk800/base.pc', 0, 0, 0)
F.deliver({t = 'hello', proto = 1, gta_build = '3889', session = 77})
frames(1)
check('main loaded on hello', F.loaded.beamls_main ~= nil)
check('level load requested', F.count('startFreeroam') == 1 and F.last('startFreeroam')[2]:find('smallgrid'))
local acks = F.sentOf('hello_ack')
check('hello acknowledged (not ready yet)', #acks == 1 and acks[1].ready == false and acks[1].session == 77)
frames(40)
acks = F.sentOf('hello_ack')
check('ready ack after level load', #acks == 2 and acks[2].ready == true, #acks)
check('sky hidden', F.scene.sunsky.hidden == true)
check('ground plane hidden', F.scene.theGroundPlane.hidden == true)
check('key colour background', F.scene.theLevelInfo.fields.canvasClearColor == '255 0 255 255')
check('UI apps hidden', F.guihooks[1] and F.guihooks[1][1] == 'ShowApps' and F.guihooks[1][2] == false)
check('ocean created', F.count('createObject') >= 1)
check('vehicle_info sent', #F.sentOf('vehicle_info') >= 1 and F.sentOf('vehicle_info')[1].model == 'etk800')

-- 3. car state every frame, GTA convention -----------------------------------------------------------
local before = #F.sentOf('car')
frames(5)
local cars = F.sentOf('car')
check('car sent every frame', #cars - before == 5, #cars - before)
local c = cars[#cars]
-- the fake car faces north (+Y) in BeamNG world: GTA heading 0 = identity rotation
check('car rotation in GTA convention', sameRot({x = c.qx, y = c.qy, z = c.qz, w = c.qw}, {x = 0, y = 0, z = 0, w = 1}), string.format('%f %f %f %f', c.qx, c.qy, c.qz, c.qw))
check('car box from OOBB', near(c.hy, 2.35) and near(c.hx, 0.95))

-- 4. camera follows GTA ------------------------------------------------------------------------------
F.deliver({t = 'mode', state = 'onfoot', reason = 'test'})
local q = mathx.norm(mathx.mul(mathx.rotZ(math.rad(30)), {x = math.sin(math.rad(-5)), y = 0, z = 0, w = math.cos(math.rad(-5))}))
F.deliver({t = 'cam', seq = 9, px = -1200.5, py = 300.25, pz = 35.0, qx = q.x, qy = q.y, qz = q.z, qw = q.w, fov = 50, near = 0.15, w = 1920, h = 1080})
frames(1)
check('free camera set', F.count('setFreeCamera') == 1)
check('camera position = GTA camera', F.cam and near(F.cam[1], -1200.5) and near(F.cam[2], 300.25) and near(F.cam[3], 35.0))
check('camera rotation = GTA camera', F.cam and sameRot({x = F.cam[4], y = F.cam[5], z = F.cam[6], w = F.cam[7]}, q))
check('camera fov vertical passthrough', near(F.fov, 50))
check('car carries cam_seq', F.sentOf('car')[#F.sentOf('car')].cam_seq == 9)
check('parked on foot (parking brake)', F.player.inputs and F.player.inputs.parkingbrake and F.player.inputs.parkingbrake[1] == 1)

-- 5. spawn_at places the car next to the player ------------------------------------------------------
F.deliver({t = 'spawn_at', x = 100, y = 200, z = 30, heading = 90})
frames(1)
local tp = F.last('safeTeleport')
check('teleported to GTA spot (+0.3 m drop)', tp and near(tp[3], 100) and near(tp[4], 200) and near(tp[5], 30.3))
-- GTA heading 90 (facing west) -> BeamNG vehicle rotation = Rz(90+180)
check('teleport rotation in BeamNG convention', tp and sameRot({x = tp[6], y = tp[7], z = tp[8], w = tp[9]}, mathx.rotZ(math.rad(270))))

-- 6. driving input ----------------------------------------------------------------------------------
F.deliver({t = 'mode', state = 'drive', reason = 'test'})
F.deliver({t = 'input', seq = 1, throttle = 0.8, brake = 0, steer = -0.5, handbrake = 0, horn = true, lights = false})
frames(1)
local inp = F.player.inputs
check('throttle applied', inp and near(inp.throttle[1], 0.8) and inp.throttle[2] == 'beamls')
check('steering applied', inp and near(inp.steering[1], -0.5))
check('handbrake released', inp and near(inp.parkingbrake[1], 0))
check('horn on', F.player.horn == true)
check('BeamNG local input disabled for throttle', F.player.allowed and F.player.allowed['throttle/local'] == false)
F.deliver({t = 'input', seq = 2, throttle = 7, brake = 0 / 0, steer = -3, handbrake = 0, horn = true, lights = true})
frames(1)
check('throttle clamped', near(F.player.inputs.throttle[1], 1))
check('NaN brake becomes 0', near(F.player.inputs.brake[1], 0))
check('steer clamped', near(F.player.inputs.steering[1], -1))
check('lights toggled', F.player.lights == true)
check('no vehicle Lua errors', not F.logged('VE E'))

-- 7. ground slabs -------------------------------------------------------------------------------------
local n, step = 3, 2.0
local z = {}
for j = 0, n - 1 do for i = 0, n - 1 do z[#z + 1] = 10.0 end end
F.deliver({t = 'ground', seq = 1, ox = 100, oy = 200, step = step, n = n, z = z})
frames(1)
local slabs = {}
for _, o in ipairs(F.objects) do if o.class == 'TSStatic' and o.pos and o.pos[3] > -1000 then slabs[#slabs + 1] = o end end
check('flat 3x3 grid -> 4 slabs', #slabs == 4, #slabs)
local s1 = slabs[1]
check('slab top at ground height', s1 and near(s1.pos[3] + s1.scale[3] / 2, 10.0, 1e-3), s1 and s1.pos[3])
check('slab shape is ours', s1 and s1.fields.shapeName == S.slab_shape)
check('collision rebuilt', F.count('reloadStaticCollision') >= 1)
-- sloped ground: z = 0.5 * x
z = {}
for j = 0, n - 1 do for i = 0, n - 1 do z[#z + 1] = 0.5 * (300 + i * step) end end
F.deliver({t = 'ground', seq = 2, ox = 300, oy = 0, step = step, n = n, z = z})
frames(1)
local sloped
for _, o in ipairs(F.objects) do if o.class == 'TSStatic' and o.pos and o.pos[1] > 290 and o.pos[1] < 310 then sloped = o break end end
local up = sloped and mathx.rotate(mathx.q(sloped.rot[1], sloped.rot[2], sloped.rot[3], sloped.rot[4]), {x = 0, y = 0, z = 1})
local nl = math.sqrt(0.25 + 1)
check('sloped slab tilted to the ground normal', up and near(up.x, -0.5 / nl, 1e-3) and near(up.z, 1 / nl, 1e-3), up and string.format('%f %f %f', up.x, up.y, up.z))
-- missing heights leave a hole
z = {}
for j = 0, n - 1 do for i = 0, n - 1 do z[#z + 1] = -10000 end end
local countBefore = beamls_main and F.loaded.beamls_main and #F.objects
F.deliver({t = 'ground', seq = 3, ox = 900, oy = 900, step = step, n = n, z = z})
frames(1)
check('no slabs where GTA found no ground', #F.objects == countBefore)

-- 8. walls ------------------------------------------------------------------------------------------
F.deliver({t = 'walls', seq = 1, hits = {{105, 200, 31, -1, 0, 0, 1, 0}, {105, 200.1, 31, -1, 0, 0, 1, 0}, {90, 190, 30.5, 0, 1, 0, 2, 0.6}, {95, 195, 30, 0, 0, 1, 1, 0}}})
frames(1)
local walls, posts = {}, {}
for _, o in ipairs(F.objects) do
  if o.class == 'TSStatic' and o.scale and o.pos[3] > -1000 then
    if near(o.scale[3], 3.0) then walls[#walls + 1] = o elseif near(o.scale[3], 1.5) then posts[#posts + 1] = o end
  end
end
check('one wall slab for two hits on the same spot', #walls == 1, #walls)
check('floor-facing hit is not a wall', #walls == 1)
local w = walls[1]
check('wall slab behind the surface', w and near(w.pos[1], 105 + 0.4, 1e-3), w and w.pos[1])
local wf = w and mathx.rotate(mathx.q(w.rot[1], w.rot[2], w.rot[3], w.rot[4]), {x = 0, y = 1, z = 0})
check('wall slab thickness axis points into the wall', wf and near(wf.x, 1, 1e-3))
check('prop post sized from the prop', #posts == 1 and near(posts[1].scale[1], 1.2))

-- 9. traffic proxies ---------------------------------------------------------------------------------
local tq = mathx.rotZ(0)
F.deliver({t = 'traffic', seq = 1, cars = {{501, 1, 110, 205, 30.8, tq.x, tq.y, tq.z, tq.w, 0, 10, 0, 4.7, 1.9, 1.5}, {502, 18, 120, 205, 30.8, 0, 0, 0, 1, 0, 0, 0, 7.5, 2.4, 3.1}}})
frames(1)
check('two proxies spawned', F.count('spawnNewVehicle') == 2, F.count('spawnNewVehicle'))
local medium, truck
for _, c2 in ipairs(F.calls) do
  if c2[1] == 'spawnNewVehicle' then
    if c2[3]:find('medium') then medium = true end
    if c2[3]:find('truck') then truck = true end
    check('proxy does not take the player seat', c2[4] == false)
  end
end
check('sedan -> medium proxy, box truck -> truck proxy', medium and truck)
check('player car stays the player vehicle', F.count('enterVehicle') >= 1)
check('proxy follows GTA pose', F.count('setClusterPosRelRot') >= 2)
frames(30)
check('proxies kept while recently seen', F.count('deleteVehicle') == 0)
frames(40)
check('stale proxies removed after the obstacle lifetime', F.count('deleteVehicle') == 2, F.count('deleteVehicle'))

-- 10. menus -----------------------------------------------------------------------------------------
F.deliver({t = 'mode', state = 'menu_vehicle', reason = 'F6'})
frames(1)
check('physics paused for the menu', F.simPaused == true)
local opened
for _, g in ipairs(F.guihooks) do if g[1] == 'ChangeState' and g[2].state == 'menu.vehicles' then opened = true end end
check('vehicle selector opened', opened)
local camCalls = F.count('camSetPosRot')
F.deliver({t = 'cam', seq = 10, px = 0, py = 0, pz = 0, qx = 0, qy = 0, qz = 0, qw = 1, fov = 50, near = 0.1, w = 1920, h = 1080})
frames(1)
check('GTA camera not forced while the menu is open', F.count('camSetPosRot') == camCalls)
F.player.model = 'pickup' -- the player picked another car
beamls_main.onKey('back_to_gta')
frames(1)
local closed = F.sentOf('menu_closed')
check('menu_closed sent with changed=true', #closed == 1 and closed[1].changed == true)
check('GTA allowed to take focus back', F.count('AllowSetForegroundWindow') == 1)
F.deliver({t = 'mode', state = 'menu_tuning', reason = 'F7'})
frames(1)
local tuning
for _, g in ipairs(F.guihooks) do if g[1] == 'ChangeState' and g[2].state == 'menu.vehicleconfig.tuning' then tuning = true end end
check('tuning menu opened', tuning)
beamls_main.onUiChangedState('play')
frames(1)
check('leaving the menu UI closes it', #F.sentOf('menu_closed') == 2 and F.sentOf('menu_closed')[2].changed == false)
F.deliver({t = 'mode', state = 'drive', reason = 'back'})
frames(1)
check('physics resumes on drive', F.simPaused == false)

-- 11. repair and time ----------------------------------------------------------------------------------
F.deliver({t = 'repair', garage = 'lsc_burton'})
frames(1)
check('repair resets physics in place', F.player.resets == 1 and F.count('resetBrokenFlexMesh') == 1)
F.deliver({t = 'time', hour = 0, minute = 0})
frames(1)
check('midnight -> time of day 0.5', near(F.tod, 0.5))
F.deliver({t = 'time', hour = 12, minute = 0})
frames(1)
check('noon -> time of day 0', near(F.tod, 0))

-- 12. messages from someone else are ignored ---------------------------------------------------------
local carsBefore = F.count('safeTeleport')
F.deliver({t = 'spawn_at', x = 1, y = 2, z = 3, heading = 0}, 60000)
frames(1)
check('spawn_at from a stranger ignored', F.count('safeTeleport') == carsBefore)
F.inbox[#F.inbox + 1] = {data = 'not json', ip = '127.0.0.1', port = 50123}
F.inbox[#F.inbox + 1] = {data = '{"t":"car","seq":1}', ip = '127.0.0.1', port = 50123}
frames(1)
check('garbage counted, own message type ignored', beamls_listener.stats.bad >= 1)

-- 13. bye ends the session cleanly --------------------------------------------------------------------
F.deliver({t = 'bye', reason = 'quit'})
frames(1)
check('main unloaded after bye', F.loaded.beamls_main == nil)
check('listener still running', F.loaded.beamls_listener ~= nil and F.bound ~= nil)
check('BeamNG input given back', F.player.allowed['throttle/local'] == true)
local alive = 0
for _, o in ipairs(F.objects) do if o.class == 'TSStatic' and not o.deleted then alive = alive + 1 end end
check('all slabs deleted', alive == 0, alive)
check('physics unpaused', F.simPaused == false)
check('bngapi report logged', F.logged('bngapi ok:'))

-- 14. a new GTA run starts a new session; protocol mismatch is refused ----------------------------
F.deliver({t = 'hello', proto = 99, gta_build = '3889', session = 1}, 50200)
frames(1)
local st = F.sentOf('status')
check('protocol mismatch reported', st[#st].state == 'error' and st[#st].msg:find('protocol'))
check('no session on mismatch', F.loaded.beamls_main == nil)
F.deliver({t = 'hello', proto = 1, gta_build = '3889', session = 2}, 50201)
frames(40)
check('new session after GTA restart', F.loaded.beamls_main ~= nil)

-- 15. GTA goes silent -> session ends ------------------------------------------------------------------
frames(60 * 4)
check('session ended after peer timeout', F.loaded.beamls_main == nil)

-- 16. port already taken: retry, no crash ---------------------------------------------------------------
extensions.unload('beamls_listener')
F.portBusy = true
extensions.load('beamls/listener')
check('busy port tolerated', F.loaded.beamls_listener ~= nil and F.logged('cannot bind'))
F.portBusy = false
frames(60 * 6)
check('binds once the port is free', F.bound ~= nil)

print(string.format('beamng lua tests: %d passed, %d failed', passed, failed))
os.exit(failed == 0 and 0 or 1)
