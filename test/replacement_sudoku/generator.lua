local core = dofile("cp_core.lua")
local cases = dofile("oracle_cases.lua")
local function pack(g)
  local t = {}
  for i = 1, 81 do t[i] = tostring(g[i]) end
  return table.concat(t)
end
-- Independent uniqueness counter: MRV backtracking, no human techniques.
local function solutions(g, limit)
  local best, digits, fewest = nil, nil, 10
  for i = 1, 81 do
    if g[i] == 0 then
      local used, possible = {}, {}
      local row, col = (i-1)//9, (i-1)%9
      for j = 1, 81 do
        local r, c = (j-1)//9, (j-1)%9
        if row == r or col == c or (row//3 == r//3 and col//3 == c//3) then
          used[g[j]] = true
        end
      end
      for d = 1, 9 do if not used[d] then possible[#possible+1] = d end end
      if #possible == 0 then return 0 end
      if #possible < fewest then best,digits,fewest = i,possible,#possible end
      if fewest == 1 then break end
    end
  end
  if not best then return 1 end
  local count = 0
  for _, d in ipairs(digits) do
    g[best] = d
    count = count + solutions(g, limit-count)
    if count >= limit then break end
  end
  g[best] = 0
  return count
end
local count = 0
local slowest = 0
local incremental = {}
for _, case in ipairs(cases) do
  if case.ceiling == 10 then
    local started = os.clock()
    local given, solution, report = core.generate(case.level, case.seed, 1)
    slowest = math.max(slowest, os.clock()-started)
    assert(given and solution, "source accepted seed must generate in one attempt")
    assert(pack(given) == case.given, "source generator exact clue parity")
    assert(pack(solution) == case.solution, "source generator exact solution parity")
    assert(report.level == case.level and report.hardest == case.hardest, "measured grade parity")
    assert(report.seed == case.seed, "zero/signed replay seed parity")
    assert(report.rng == case.rng, "unsigned xorshift continuation parity")
    for i = 1, 81 do
      assert((given[i] == 0) == (given[82-i] == 0), "rotational clue symmetry")
      assert(given[i] == 0 or given[i] == solution[i], "clue matches solution")
    end
    assert(solutions(given, 2) == 1, "independent uniqueness proof")
    if not incremental[case.level] then
      incremental[case.level] = true
      local job = core.newGenerator(case.level, case.seed)
      local steps = 0
      while not job.done do
        local guard, at = job.guard, job.at
        core.stepGenerator(job)
        assert(job.guard-guard <= 128, "unbounded fill callback")
        if at then assert(job.at-at <= 1, "more than one pair removal in callback") end
        steps=steps+1;assert(steps < 1700, "generation exceeded fill budget")
      end
      assert(job.accepted and pack(job.given)==case.given, "incremental/source parity")
      assert(core.stepGenerator(job), "completed task must remain complete")
      assert(pack(job.given)==case.given, "completed task changed state")
      local wrong = core.newGenerator((case.level+1)%4,case.seed)
      while not core.stepGenerator(wrong) do end
      assert(not wrong.accepted, "wrong grade must be rejected, never relabelled")
    end
    count = count + 1
    collectgarbage("collect")
  end
end
collectgarbage('collect')
local baseline=collectgarbage('count')
for i=1,64 do
  local cancelled=core.newGenerator(0,i)
  core.stepGenerator(cancelled)
  cancelled=nil
  collectgarbage('collect')
end
assert(collectgarbage('count') < baseline+8, 'cancelled generation retained jobs')
print("source generator and independent uniqueness cases", count)
print("slowest one-attempt generation host CPU seconds (not hardware)", slowest)
