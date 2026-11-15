-- 2048 game for CrossSmudge (Lua)
-- Faithful reproduction of TwoZeroFourEightActivity.h

local ui = smudge.dofile("ui.lua")
local best = 0
local board = {}
local score = 0
local gameOver = false
local showResetConfirm = false
local w, h = 480, 800

local function init_board()
    board = {}
    for r = 1, 4 do
        board[r] = {}
        for c = 1, 4 do
            board[r][c] = 0
        end
    end
    score = 0
    gameOver = false
    showResetConfirm = false
end

local empty_r = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
local empty_c = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
local non_zero = {0, 0, 0, 0}
local res = {0, 0, 0, 0}

local function spawn_tile()
    local empty_count = 0
    for r = 1, 4 do
        for c = 1, 4 do
            if board[r][c] == 0 then
                empty_count = empty_count + 1
                empty_r[empty_count] = r
                empty_c[empty_count] = c
            end
        end
    end
    if empty_count == 0 then return false end
    local idx = math.random(1, empty_count)
    board[empty_r[idx]][empty_c[idx]] = (math.random() < 0.9) and 2 or 4
    return true
end

local function can_move()
    for r = 1, 4 do
        for c = 1, 4 do
            if board[r][c] == 0 then return true end
            if c < 4 and board[r][c] == board[r][c + 1] then return true end
            if r < 4 and board[r][c] == board[r + 1][c] then return true end
        end
    end
    return false
end

local function reset_game()
    init_board()
    spawn_tile()
    spawn_tile()
end

-- Two validated copies let a damaged Best key recover from the other copy.
local function valid_best(value)
    if type(value) ~= "string" or not value:match("^%d+$") then return 0 end
    local n = tonumber(value)
    if not n or n < 0 or n > 2147483647 or n % 1 ~= 0 then return 0 end
    return math.floor(n)
end

local function update_best()
    best = math.max(best, score, valid_best(smudge.load("best_score", "")),
                    valid_best(smudge.load("best_score_backup", "")))
end

local function save_best()
    update_best()
    local value = tostring(best)
    for _, key in ipairs({"best_score_backup", "best_score"}) do
        if smudge.load(key, "") ~= value then
            smudge.save(key, value)
            if smudge.load(key, "") ~= value then
                smudge.log("2048: could not persist " .. key)
            end
        end
    end
end

local function save_state()
    save_best()
    local flat = {}
    for r = 1, 4 do
        for c = 1, 4 do
            table.insert(flat, tostring(board[r][c]))
        end
    end
    smudge.save("board", table.concat(flat, ","))
    smudge.save("score", tostring(score))
    smudge.save("game_over", gameOver and "1" or "0")
end

local function load_state()
    local s = smudge.load("board", "")
    if s and #s > 0 then
        local parts = {}
        for p in string.gmatch(s, "([^,]+)") do
            local value = tonumber(p)
            if not value or value < 0 or value > 1073741824 or value % 1 ~= 0 then return false end
            local power = value
            while power > 1 and power % 2 == 0 do power = power / 2 end
            if value ~= 0 and (value < 2 or power ~= 1) then return false end
            table.insert(parts, math.floor(value))
        end
        if #parts == 16 then
            board = {}
            for r = 1, 4 do
                board[r] = {}
                for c = 1, 4 do
                    board[r][c] = parts[(r - 1) * 4 + c]
                end
            end
            score = valid_best(smudge.load("score", "0"))
            gameOver = (smudge.load("game_over", "0") == "1")
            return true
        end
    end
    return false
end

function on_init()
    w, h = smudge.get_bounds()
    ui.init()
    math.randomseed(os.time())
    if not load_state() then
        reset_game()
    end
    update_best()
end

local function slide_and_merge(a, b, c, d)
    local nz_count = 0
    if a ~= 0 then nz_count = nz_count + 1; non_zero[nz_count] = a end
    if b ~= 0 then nz_count = nz_count + 1; non_zero[nz_count] = b end
    if c ~= 0 then nz_count = nz_count + 1; non_zero[nz_count] = c end
    if d ~= 0 then nz_count = nz_count + 1; non_zero[nz_count] = d end

    res[1], res[2], res[3], res[4] = 0, 0, 0, 0
    local res_count = 0
    local skip = false
    local gained = 0

    for i = 1, nz_count do
        if skip then
            skip = false
        elseif i < nz_count and non_zero[i] == non_zero[i + 1] then
            local merged = non_zero[i] * 2
            res_count = res_count + 1
            res[res_count] = merged
            gained = gained + merged
            skip = true
        else
            res_count = res_count + 1
            res[res_count] = non_zero[i]
        end
    end

    return gained
end

local function move(dr, dc)
    if gameOver or showResetConfirm then return end
    local moved = false
    local total_gained = 0

    if dc == -1 then -- Left
        for r = 1, 4 do
            local g = slide_and_merge(board[r][1], board[r][2], board[r][3], board[r][4])
            total_gained = total_gained + g
            for c = 1, 4 do
                if board[r][c] ~= res[c] then moved = true end
                board[r][c] = res[c]
            end
        end
    elseif dc == 1 then -- Right
        for r = 1, 4 do
            local g = slide_and_merge(board[r][4], board[r][3], board[r][2], board[r][1])
            total_gained = total_gained + g
            for c = 1, 4 do
                if board[r][5 - c] ~= res[c] then moved = true end
                board[r][5 - c] = res[c]
            end
        end
    elseif dr == -1 then -- Up
        for c = 1, 4 do
            local g = slide_and_merge(board[1][c], board[2][c], board[3][c], board[4][c])
            total_gained = total_gained + g
            for r = 1, 4 do
                if board[r][c] ~= res[r] then moved = true end
                board[r][c] = res[r]
            end
        end
    elseif dr == 1 then -- Down
        for c = 1, 4 do
            local g = slide_and_merge(board[4][c], board[3][c], board[2][c], board[1][c])
            total_gained = total_gained + g
            for r = 1, 4 do
                if board[5 - r][c] ~= res[r] then moved = true end
                board[5 - r][c] = res[r]
            end
        end
    end

    if moved then
        score = score + total_gained
        update_best()
        spawn_tile()
        if not can_move() then
            gameOver = true
        end
    end
end

function on_draw()
    w, h = smudge.get_bounds()
    ui.draw(board, score, best, gameOver, showResetConfirm, w, h)
end

function on_button(btn, pressed)
    if not pressed then return end

    if showResetConfirm then
        if btn == "back" then
            showResetConfirm = false
        elseif btn == "confirm" then
            save_best()
            reset_game()
            showResetConfirm = false
            save_state()
        end
        return
    end

    if btn == "back" then
        save_state()
        smudge.exit()
    elseif btn == "confirm" then
        showResetConfirm = true
    elseif btn == "left" then
        move(-1, 0) -- Bottom button labeled "Up" moves UP
    elseif btn == "right" then
        move(1, 0)  -- Bottom button labeled "Down" moves DOWN
    elseif btn == "up" or btn == "page_back" then
        move(0, -1) -- Top side button moves LEFT
    elseif btn == "down" or btn == "page_forward" then
        move(0, 1)  -- Bottom side button moves RIGHT
    end
end

function on_swipe(dir)
    if dir == "up" then
        move(-1, 0)
    elseif dir == "down" then
        move(1, 0)
    elseif dir == "left" then
        move(0, -1)
    elseif dir == "right" then
        move(0, 1)
    end
end

function on_tap(x, y)
    w, h = smudge.get_bounds()
    local action = ui.hit(x, y, showResetConfirm, w, h)
    if action then on_button(action, true) end
end

function on_exit()
    save_state()
end
