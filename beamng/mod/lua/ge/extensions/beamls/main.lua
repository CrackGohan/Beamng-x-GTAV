-- beamls_main: the BeamNG side of a running mashup session. Loaded by beamls_listener on GTA's hello,
-- it routes bridge messages to the modules and sends the car's state back every frame.
-- (sheet systems: bng_main)
local M = {}

local req = rawget(_G, 'beamls_req') or function(n) return require('beamls/' .. n) end
local api = req('gen/bngapi')
local S = req('gen/settings')

local ctx = {
  api = api,
  S = S,
  proto = req('gen/protocol'),
  obstacles = req('gen/obstacles'),
  traffic_proxies = req('gen/traffic_proxies'),
  keys = req('gen/keys'),
  mathx = req('mathx'),
}
local session = req('session')
local camera = req('camera')
local car = req('player_car')
local input = req('input')
local world = req('world')
local traffic = req('traffic')
local menu = req('menu')
ctx.session, ctx.camera, ctx.car, ctx.input, ctx.world, ctx.traffic, ctx.menu = session, camera, car, input, world, traffic, menu

M.mode = 'onfoot'
M.latest = {}       -- state messages: newest of each type, applied once per frame
local pending = {}  -- event messages, applied in order
local statusTimer = 0
local lastStatus
local helloSession = 0

local STATE_MSGS = {cam = true, input = true, ground = true, walls = true, traffic = true, time = true}

function ctx.info(msg) if log then log('I', 'beamls', msg) end end
function ctx.send(t, m) return beamls_listener and beamls_listener.send(t, m) end
function ctx.status(state, msg)
  lastStatus = state .. '|' .. msg
  ctx.send('status', {state = state, msg = msg})
end
function ctx.onReady()
  world.ensureWater()
  ctx.send('hello_ack', {proto = ctx.proto.proto, bng_version = tostring(rawget(_G, 'beamng_versionb') or '?'), session = helloSession, ready = true})
  ctx.status('ready', 'BeamNG ready')
  local info = car.info(0, true)
  if info then ctx.send('vehicle_info', info) end
end

for _, m in ipairs({session, camera, car, input, world, traffic, menu}) do m.init(ctx) end

function M.onExtensionLoaded()
  ctx.info('beamls_main loaded')
  session.start()
end

local function applyMode(state)
  M.mode = state
  if state == 'menu_vehicle' then
    menu.show('vehicles')
  elseif state == 'menu_tuning' then
    menu.show('tuning')
  elseif state == 'paused' then
    session.setPaused(true)
  elseif state == 'off' then
    input.park()
    session.setPaused(true)
  else -- drive, onfoot
    if menu.open then menu.close('gta took over') end
    session.setPaused(false)
    if state == 'onfoot' then input.park() end
  end
end

function M.onMessage(m)
  if m.t == 'hello' then
    helloSession = m.session
    ctx.send('hello_ack', {proto = ctx.proto.proto, bng_version = tostring(rawget(_G, 'beamng_versionb') or '?'), session = m.session, ready = session.ready()})
  elseif STATE_MSGS[m.t] then
    M.latest[m.t] = m
  else
    pending[#pending + 1] = m
  end
end

local function handleEvent(m)
  if m.t == 'mode' then
    if m.state ~= M.mode then ctx.info('mode ' .. M.mode .. ' -> ' .. m.state .. ' (' .. m.reason .. ')') end
    applyMode(m.state)
  elseif m.t == 'spawn_at' then
    car.spawnAt(m)
  elseif m.t == 'repair' then
    car.repair()
    ctx.info('repair at ' .. m.garage)
  elseif m.t == 'bye' then
    M.onSessionEnd('bye: ' .. m.reason)
  end
end

function M.onUpdate(dtReal, dtSim)
  local dt = dtReal or 0
  session.update(dt)
  if not session.ready() then return end

  for i = 1, #pending do handleEvent(pending[i]) end
  pending = {}

  local L = M.latest
  if L.cam and not menu.open then camera.apply(L.cam) end
  if L.input and M.mode == 'drive' then input.apply(L.input) end
  if L.ground then world.ground(L.ground) end
  if L.walls then world.walls(L.walls) end
  if L.traffic then traffic.update(L.traffic) end
  if L.time then session.setTime(L.time) end
  M.latest = {}

  world.update(dt)
  traffic.follow(session.paused and 0 or (dtSim or dt))

  local c = car.sample(dt, camera.lastSeq)
  if c then ctx.send('car', c) end
  local info = car.info(dt)
  if info then ctx.send('vehicle_info', info) end

  statusTimer = statusTimer - dt
  if statusTimer <= 0 then
    statusTimer = 1
    if menu.open then ctx.status('menu', menu.open) else ctx.status('ready', 'cars ' .. traffic.count() .. ', slabs ' .. world.count()) end
  end
end

-- Key from our BeamNG input action (sheet keys, side beamng).
function M.onKey(id)
  if id == 'back_to_gta' then menu.close('key') end
end

-- BeamNG UI screen changes; the hook name comes from the bng_api sheet (row ui_state_hook).
M[api.ui_state_hook() or 'onUiChangedState'] = function(newState) menu.onUiState(newState) end

function M.onSessionEnd(reason)
  ctx.info('beamls session end: ' .. tostring(reason))
  if menu.open then menu.close('session end') end
  input.release()
  traffic.clear()
  world.clear()
  session.stop()
  local ok, failed = api.report()
  ctx.info('bngapi ok: ' .. table.concat(ok, ', '))
  if #failed > 0 then ctx.info('bngapi failed: ' .. table.concat(failed, '; ')) end
  if extensions and extensions.unload then pcall(extensions.unload, 'beamls_main') end
end

return M
