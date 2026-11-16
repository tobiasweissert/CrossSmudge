-- Sticky-only presentation. Masks use LuaRunner's transparent, MSB-first sprite ABI.
local UI = {}
local fonts
function UI.init() fonts = smudge.dofile("fonts.lua") end
local function measure(text, font)
    local width = 0
    for i = 1, #text do width = width + (font.glyphs[text:byte(i)] or font.glyphs[63])[5] end
    return width
end
local sizes = {52, 40, 32, 24, 20, 16, 12, 8, 6}
local function label(text, x, y, maxWidth, size, style, align, white, gray)
    local font = fonts[style .. size]
    if measure(text, font) > maxWidth then
        for _, s in ipairs(sizes) do
            if s <= size then
                local candidate = fonts[style .. s] or fonts["b" .. s]
                font = candidate
                if measure(text, candidate) <= maxWidth then break end
            end
        end
    end
    local width = measure(text, font)
    if align == "center" then x = x - math.floor(width / 2)
    elseif align == "right" then x = x - width end
    -- Align the actual cap-height ink, not a guessed renderer line height.
    local cap = font.glyphs[string.byte("0")]
    local baseline = math.floor(y - (cap[4] + cap[2] / 2))
    for i = 1, #text do
        local g = font.glyphs[text:byte(i)] or font.glyphs[63]
        smudge.draw_sprite(math.floor(x + g[3]), baseline + g[4], g[1], g[2], g[6], white or false, gray or false)
        x = x + g[5]
    end
end
local function span(x, y, width, height, tone)
    if tone == "light" or tone == "dark" then
        smudge.rect_dither(x, y, width, height, tone == "dark")
    else
        smudge.rect(x, y, width, height, true, tone == "black")
    end
end
-- Rounded scanlines use supported rect/dither APIs; corners retain the same pattern phase.
local function rounded(x, y, width, height, radius, tone)
    radius = math.min(radius, math.floor(height / 2), math.floor(width / 2))
    for row = 0, radius - 1 do
        local dy = radius - row - 0.5
        local inset = math.ceil(radius - math.sqrt(radius * radius - dy * dy))
        span(x + inset, y + row, width - inset * 2, 1, tone)
        span(x + inset, y + height - 1 - row, width - inset * 2, 1, tone)
    end
    span(x, y + radius, width, height - radius * 2, tone)
end
local function outline(x, y, width, height, radius, stroke, tone)
    rounded(x, y, width, height, radius, tone)
    rounded(x + stroke, y + stroke, width - 2 * stroke, height - 2 * stroke,
            math.max(0, radius - stroke), "white")
end
local function comma(n)
    local s = tostring(n)
    local result = s
    while true do
        local nextValue, count = result:gsub("^(%d+)(%d%d%d)", "%1,%2")
        result = nextValue
        if count == 0 then return result end
    end
end
function UI.geometry(w, h)
    local landscape = w > h
    local gap = landscape and 7 or 9
    local cell = math.floor((math.min(w - 32, h - 180) - gap * 3) / 4)
    local side = cell * 4 + gap * 3
    local y = landscape and 89 or math.floor((h - side) / 2)
    return {x=math.floor((w-side)/2), y=y, cell=cell, gap=gap,
            bx=w-202, by=h-90, bw=178, bh=66}
end
local function inside(x, y, bx, by, bw, bh)
    return x >= bx and x < bx + bw and y >= by and y < by + bh
end
function UI.hit(x, y, modal, w, h)
    if modal then
        local mx, my = math.floor((w-360)/2), math.floor((h-180)/2)
        if inside(x,y,mx+16,my+108,156,56) then return "back" end
        if inside(x,y,mx+188,my+108,156,56) then return "confirm" end
    else
        local g = UI.geometry(w,h)
        if inside(x,y,g.bx,g.by,g.bw,g.bh) then return "confirm" end
    end
end
function UI.draw(board, score, best, over, modal, w, h)
    smudge.clear()
    local g = UI.geometry(w,h)
    label("2048",24,56,w/2-30,52,"b","left")
    label(comma(score),w-24,56,w/2-30,52,"r","right")
    for r = 1,4 do
        for c = 1,4 do
            local x, y = g.x+(c-1)*(g.cell+g.gap), g.y+(r-1)*(g.cell+g.gap)
            local value = board[r][c]
            if value == 0 then outline(x,y,g.cell,g.cell,10,2,"light")
            elseif value <= 4 then outline(x,y,g.cell,g.cell,10,3,"black")
            else rounded(x,y,g.cell,g.cell,10,value <= 16 and "light" or value <= 64 and "dark" or "black") end
            if value ~= 0 then
                label(tostring(value),x+math.floor(g.cell/2),y+g.cell/2,g.cell-16,52,"b","center",value>=32)
            end
        end
    end
    label("Best "..comma(best),24,g.by+g.bh/2,g.bx-40,24,"r","left")
    outline(g.bx,g.by,g.bw,g.bh,33,2,"black")
    label("New game",g.bx+g.bw/2,g.by+g.bh/2,g.bw-24,24,"b","center")
    if over then label("GAME OVER",w/2,g.y-20,w-48,20,"b","center") end
    if modal then
        local mx, my = math.floor((w-360)/2), math.floor((h-180)/2)
        outline(mx,my,360,180,16,3,"black")
        label("Reset game?",w/2,my+48,328,32,"b","center")
        outline(mx+16,my+108,156,56,28,2,"black")
        outline(mx+188,my+108,156,56,28,2,"black")
        label("Cancel",mx+94,my+136,132,24,"b","center")
        label("New game",mx+266,my+136,132,24,"b","center")
    end
end
return UI
