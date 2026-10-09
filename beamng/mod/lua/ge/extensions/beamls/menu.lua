-- Menu: open BeamNG's vehicle selector or tuning menu when GTA asks (F6 / F7), with physics paused and the
-- UI visible; when the player is done (F6/F7 in BeamNG, or the UI leaves the menu) hide the UI again, let
-- GTA take keyboard focus back and tell GTA whether the car changed. (sheet systems: bng_menu)
local M = {}

local ctx
M.open = nil -- nil, 'vehicles' or 'tuning'
local before

function M.init(c)
  ctx = c
  M.open = nil
end

local function carKey()
  local info = ctx.car.info(0, true)
  if not info then return '' end
  return string.format('%s|%s', info.model, info.config)
end

function M.show(kind)
  if M.open == kind then return end
  before = carKey()
  M.open = kind
  ctx.camera.setEnabled(false)
  ctx.session.setPaused(true)
  ctx.api.ui_show()
  if kind == 'tuning' then ctx.api.ui_open_tuning() else ctx.api.ui_open_vehicles() end
  ctx.info('menu opened: ' .. kind)
end

function M.close(reason)
  if not M.open then return end
  local kind = M.open
  M.open = nil
  ctx.api.ui_hide()
  ctx.camera.setEnabled(true)
  ctx.api.allow_set_foreground()
  local changed = carKey() ~= before
  ctx.send('menu_closed', {changed = changed})
  ctx.info('menu closed (' .. kind .. ', ' .. tostring(reason) .. '), car changed: ' .. tostring(changed))
  -- physics resumes when GTA sends mode drive/onfoot
end

-- BeamNG UI changed screen. A state outside the menus means the player left them.
function M.onUiState(newState)
  if not M.open or type(newState) ~= 'string' then return end
  if not string.find(newState, 'menu', 1, true) then M.close('ui ' .. newState) end
end

return M
