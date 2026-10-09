-- beamls_listener: loaded by scripts/beamls/modScript.lua in every BeamNG session. It binds the bridge port
-- on 127.0.0.1 and does nothing else until BeamLS.asi (GTA V) says hello; then it loads beamls_main and
-- forwards every datagram to it. Normal BeamNG sessions are untouched. (sheet systems: bng_listener)
local M = {}

local logTag = 'beamls'

-- Robust require for our own modules: BeamNG's package.path differs between versions, so try the
-- extension-relative names first.
local function req(name)
  local tried = {}
  for _, prefix in ipairs({'beamls/', 'ge/extensions/beamls/', 'lua/ge/extensions/beamls/'}) do
    local ok, mod = pcall(require, prefix .. name)
    if ok then return mod end
    tried[#tried + 1] = tostring(mod)
  end
  error('beamls: cannot require ' .. name .. ': ' .. table.concat(tried, ' | '))
end
rawset(_G, 'beamls_req', req)

local api = req('gen/bngapi')
local proto = req('gen/protocol')
local S = req('gen/settings')

local sock
local bindRetry = 0
local peer -- {ip = '127.0.0.1', port = n}
local clock = 0
local lastRecv = -1
local sessionActive = false

M.stats = {rx = 0, tx = 0, bad = 0}

local function info(msg) if log then log('I', logTag, msg) end end
local function warn(msg) if log then log('W', logTag, msg) end end

local function bind()
  sock = api.socket_udp()
  if not sock then return false end
  sock:settimeout(0)
  local ok, err = sock:setsockname('127.0.0.1', S.bridge_port)
  if not ok then
    warn('beamls listener cannot bind 127.0.0.1:' .. S.bridge_port .. ' (' .. tostring(err) .. '), retrying')
    pcall(function() sock:close() end)
    sock = nil
    return false
  end
  info('beamls listener on ' .. S.bridge_port)
  return true
end

-- Send a message table (already in wire form, or a type + plain table to encode).
function M.send(t, m)
  if not sock or not peer then return false end
  local wire = m and proto.encode[t](m) or t
  local data = api.json_encode(wire)
  if not data then return false end
  local ok = sock:sendto(data, peer.ip, peer.port)
  if ok then M.stats.tx = M.stats.tx + 1 end
  return ok ~= nil
end

function M.hasPeer() return peer ~= nil end
function M.clock() return clock end

local function endSession(reason)
  if not sessionActive then return end
  sessionActive = false
  info('beamls session ended: ' .. tostring(reason))
  if beamls_main and beamls_main.onSessionEnd then pcall(beamls_main.onSessionEnd, reason) end
  peer = nil
end

local function handle(data, ip, port)
  local o = api.json_decode(data)
  if type(o) ~= 'table' or type(o.t) ~= 'string' or not proto.decode[o.t] then
    M.stats.bad = M.stats.bad + 1
    return
  end
  if proto.dir[o.t] == 'b2g' then return end -- our own kind of message: ignore
  local m = proto.decode[o.t](o)
  if m.t == 'hello' then
    peer = {ip = ip, port = port}
    if m.proto ~= proto.proto then
      M.send('status', {state = 'error', msg = 'protocol ' .. m.proto .. ' != ' .. proto.proto .. ': update the mod on both games'})
      return
    end
    if not sessionActive then
      sessionActive = true
      info('beamls hello from GTA build ' .. m.gta_build .. ' session ' .. m.session)
      api.ext_load('beamls_main')
    end
  elseif not peer or ip ~= peer.ip or port ~= peer.port then
    return -- not from the GTA we are talking to
  end
  lastRecv = clock
  if beamls_main and beamls_main.onMessage then
    local ok, err = pcall(beamls_main.onMessage, m)
    if not ok then warn('beamls message ' .. m.t .. ' failed: ' .. tostring(err)) end
  end
  if m.t == 'bye' then endSession('bye: ' .. m.reason) end
end

function M.poll()
  if not sock then return end
  for _ = 1, 512 do -- bounded: never stall a frame
    local data, ip, port = sock:receivefrom()
    if not data then break end
    M.stats.rx = M.stats.rx + 1
    handle(data, ip, port)
  end
end

function M.onUpdate(dtReal)
  clock = clock + (dtReal or 0)
  if not sock then
    bindRetry = bindRetry - (dtReal or 0)
    if bindRetry <= 0 then
      bindRetry = 5
      bind()
    end
    return
  end
  M.poll()
  if sessionActive and lastRecv >= 0 and (clock - lastRecv) * 1000 > S.peer_timeout_ms then
    endSession('GTA went silent')
  end
end

function M.onExtensionLoaded()
  bind()
end

function M.onExtensionUnloaded()
  endSession('listener unloaded')
  if sock then pcall(function() sock:close() end) end
  sock = nil
end

return M
