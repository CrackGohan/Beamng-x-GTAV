-- Minimal JSON for the tests (BeamNG provides jsonEncode/jsonDecode in the game).
local M = {}

local function encStr(s)
  return '"' .. s:gsub('[%c"\\]', function(c)
    local map = {['"'] = '\\"', ['\\'] = '\\\\', ['\n'] = '\\n', ['\r'] = '\\r', ['\t'] = '\\t'}
    return map[c] or string.format('\\u%04x', c:byte())
  end) .. '"'
end

local function isArray(t)
  local n = 0
  for k in pairs(t) do
    if type(k) ~= 'number' then return false end
    n = n + 1
  end
  return n == #t
end

function M.encode(v)
  local t = type(v)
  if t == 'nil' then return 'null' end
  if t == 'boolean' then return v and 'true' or 'false' end
  if t == 'number' then
    if v ~= v or v == math.huge or v == -math.huge then return 'null' end
    if v == math.floor(v) and math.abs(v) < 1e15 then return string.format('%d', v) end
    return string.format('%.9g', v)
  end
  if t == 'string' then return encStr(v) end
  if t == 'table' then
    local out = {}
    if next(v) == nil then return '{}' end
    if isArray(v) then
      for i = 1, #v do out[i] = M.encode(v[i]) end
      return '[' .. table.concat(out, ',') .. ']'
    end
    for k, val in pairs(v) do out[#out + 1] = encStr(tostring(k)) .. ':' .. M.encode(val) end
    table.sort(out)
    return '{' .. table.concat(out, ',') .. '}'
  end
  error('cannot encode ' .. t)
end

function M.decode(s)
  local pos = 1
  local function ws() pos = s:find('[^ \t\r\n]', pos) or #s + 1 end
  local value
  local function str()
    local out = {}
    pos = pos + 1
    while true do
      local c = s:sub(pos, pos)
      if c == '' then error('unterminated string') end
      if c == '"' then pos = pos + 1 break end
      if c == '\\' then
        local e = s:sub(pos + 1, pos + 1)
        local map = {['"'] = '"', ['\\'] = '\\', ['/'] = '/', b = '\b', f = '\f', n = '\n', r = '\r', t = '\t'}
        if e == 'u' then
          out[#out + 1] = string.char(tonumber(s:sub(pos + 2, pos + 5), 16) % 256)
          pos = pos + 6
        else
          out[#out + 1] = map[e]
          pos = pos + 2
        end
      else
        out[#out + 1] = c
        pos = pos + 1
      end
    end
    return table.concat(out)
  end
  function value()
    ws()
    local c = s:sub(pos, pos)
    if c == '{' then
      local t = {}
      pos = pos + 1
      ws()
      if s:sub(pos, pos) == '}' then pos = pos + 1 return t end
      while true do
        ws()
        local k = str()
        ws()
        assert(s:sub(pos, pos) == ':', 'expected :')
        pos = pos + 1
        t[k] = value()
        ws()
        local d = s:sub(pos, pos)
        pos = pos + 1
        if d == '}' then return t end
        assert(d == ',', 'expected , in object')
      end
    elseif c == '[' then
      local t = {}
      pos = pos + 1
      ws()
      if s:sub(pos, pos) == ']' then pos = pos + 1 return t end
      while true do
        t[#t + 1] = value()
        ws()
        local d = s:sub(pos, pos)
        pos = pos + 1
        if d == ']' then return t end
        assert(d == ',', 'expected , in array')
      end
    elseif c == '"' then
      return str()
    elseif s:sub(pos, pos + 3) == 'true' then pos = pos + 4 return true
    elseif s:sub(pos, pos + 4) == 'false' then pos = pos + 5 return false
    elseif s:sub(pos, pos + 3) == 'null' then pos = pos + 4 return nil
    else
      local num = s:match('^-?%d+%.?%d*[eE]?[-+]?%d*', pos)
      assert(num and #num > 0, 'bad json at ' .. pos)
      pos = pos + #num
      return tonumber(num)
    end
  end
  local ok, v = pcall(value)
  if not ok then return nil end
  return v
end

return M
