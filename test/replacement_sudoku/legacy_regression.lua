-- This must fail against the original shipped generator. Its random digging
-- neither retains rotational clue symmetry nor proves uniqueness.
local generate = dofile("generator.lua")
math.randomseed(12345)
local board, solution, fixed = {}, {}, {}
generate(board, solution, fixed, 3)
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
assert(solutions(board, 2) == 1, "original generator produces multiple solutions")
for cell = 1, 81 do
  assert((board[cell] == 0) == (board[82-cell] == 0),
         "original generator violates CrossPlay rotational clue symmetry")
end
