assert(math.maxinteger == 2147483647 and (16777216.0 + 1.0) == 16777216.0,
       "tests require production int32/float32 Lua")
local core = dofile("cp_core.lua")
local cases = dofile("oracle_cases.lua")
local function unpack_grid(text)
  local g = {}
  for i = 1, 81 do g[i] = tonumber(text:sub(i, i)) end
  return g
end
local function packed(g)
  local t = {}
  for i = 1, 81 do t[i] = tostring(g[i]) end
  return table.concat(t)
end
for index, case in ipairs(cases) do
  local r = core.solve(unpack_grid(case.given), case.ceiling)
  for _, key in ipairs({"solved", "broken", "hardest", "steps"}) do
    assert(r[key] == case[key], "source oracle case " .. index .. " " .. key ..
           ": got " .. tostring(r[key]) .. " expected " .. tostring(case[key]))
  end
  assert(packed(r.grid) == case.grid, "source oracle case " .. index .. " cell values")
  local h = core.next_hint(unpack_grid(case.given), case.ceiling)
  assert(h.found == case.hint.found, "source hint presence case " .. index)
  if h.found then
    for _, key in ipairs({"cell", "digit", "technique", "unit"}) do
      assert(h[key] == case.hint[key], "source hint " .. key .. " case " .. index ..
             ": got " .. tostring(h[key]) .. " expected " .. tostring(case.hint[key]))
    end
  end
  collectgarbage("collect")
end
local bad = unpack_grid(string.rep("0", 81))
bad[1], bad[2] = 5, 5
local report = core.solve(bad, 10)
assert(report.broken and not report.solved, "contradictory clues must reject")
print("source-oracle solve comparisons", #cases)
