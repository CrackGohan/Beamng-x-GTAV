-- Session: load the empty level, prepare the scene so only the car renders on the key colour, pause and
-- unpause physics, and follow GTA's clock. (sheet systems: bng_session, bng_time)
local M = {}

local ctx
M.state = 'idle' -- idle -> loading -> ready -> ended
M.paused = false
local waitTime = 0
local timeSent

function M.init(c)
  ctx = c
  M.state = 'idle'
end

local function levelLoaded()
  local f = ctx.api.get_level_name()
  return f ~= nil and string.find(string.lower(f), string.lower(ctx.S.level), 1, true) ~= nil
end

function M.start()
  if levelLoaded() then
    M.state = 'loading' -- prepare next frame (objects exist already)
  else
    ctx.info('loading level ' .. ctx.S.level)
    ctx.api.start_freeroam(ctx.S.level)
    M.state = 'loading'
  end
  waitTime = 0
end

-- Hide everything except vehicles and make the background the key colour.
function M.prepare()
  local hidden = 0
  for _, name in ipairs(ctx.S.hide_objects) do
    local obj = ctx.api.find_object(name)
    if obj and ctx.api.hide_object(obj, true) then hidden = hidden + 1 end
  end
  local k = ctx.S.key_color
  ctx.api.set_clear_color(k[1], k[2], k[3])
  ctx.api.ui_hide()
  ctx.info('scene prepared, hid ' .. hidden .. ' objects')
end

function M.update(dt)
  if M.state == 'loading' then
    waitTime = waitTime + dt
    if levelLoaded() and waitTime > 0.5 then
      M.prepare()
      M.state = 'ready'
      ctx.onReady()
    elseif waitTime > 90 then
      ctx.status('error', 'level ' .. ctx.S.level .. ' did not load')
      waitTime = 0
    end
  end
end

function M.ready() return M.state == 'ready' end

function M.setPaused(p)
  if M.paused ~= p then
    M.paused = p
    ctx.api.sim_pause(p)
  end
end

-- GTA clock -> BeamNG time of day (0 = noon, 0.5 = midnight).
function M.timeOfDay(hour, minute)
  local h = (hour or 12) + (minute or 0) / 60
  local t = ((h - 12) / 24) % 1
  return t
end

function M.setTime(m)
  local t = M.timeOfDay(m.hour, m.minute)
  if not timeSent or math.abs(t - timeSent) > 1e-4 then
    timeSent = t
    ctx.api.set_time_of_day(t)
  end
end

function M.stop()
  M.setPaused(false)
  ctx.api.ui_show()
  M.state = 'ended'
end

return M
