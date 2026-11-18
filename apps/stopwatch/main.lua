-- Stopwatch
-- Pixel cartoon stopwatch with digital display and lap split recording

local U = dofile('unified_ui.lua')
local P = dofile('primary_numerals.lua')
local footer = {}
local w, h = 480, 800
if smudge and smudge.get_bounds then
    w, h = smudge.get_bounds()
end

local is_running = false
local start_time_ms = 0
local accumulated_ms = 0
local last_split_total = 0
local splits = {}
local scroll_offset = 0

local last_render_ms = 0
local plunger_down = false
local plunger_timer = 0
local split_flash_timer = 0

-- Shared draw/hit geometry: portrait keeps the original watch/card positions.
-- Landscape fits the same artwork beside the split card instead of clipping
-- its fixed portrait bottom below the Sticky screen.
local function screen_layout()
    local landscape = w > h
    local x = landscape and 320 or 14
    local y = landscape and 112 or 240
    local ch = h - 68 - y
    return {cx=landscape and 160 or w//2, cy=landscape and 180 or 140,
        timer={x=14,y=112,w=landscape and 292 or w-28,h=100},
        card_x=x,sy=y,card_w=w-14-x,card_h=ch,footer_y=h-60,
        visible_rows=math.max(1,(ch-120)//38)}
end

local function get_now_ms()
    if smudge and smudge.millis then
        return smudge.millis()
    end
    return math.floor(os.time() * 1000)
end

local notice = ""
local blocked_save = false
local MAX_SPLITS, MAX_SAVE_BYTES = 512, 65536
local lap_bytes = 0
local MAX_ELAPSED = 2147483647
local function get_elapsed_ms()
    if is_running then
        local delta = get_now_ms() - start_time_ms
        if delta < 0 or accumulated_ms > MAX_ELAPSED - delta then
            notice = "Time limit reached"
            return MAX_ELAPSED
        end
        return accumulated_ms + delta
    end
    if accumulated_ms == MAX_ELAPSED then notice = "Time limit reached" end
    return accumulated_ms
end

local function format_time(ms)
    local total_cs = math.floor(ms / 10)
    local cs = total_cs % 100
    local total_sec = math.floor(ms / 1000)
    local s = total_sec % 60
    local total_min = math.floor(total_sec / 60)
    local m = total_min % 60
    local hr = math.floor(total_min / 60)

    if hr > 0 then
        return string.format("%d:%02d:%02d.%02d", hr, m, s, cs)
    else
        return string.format("%02d:%02d.%02d", m, s, cs)
    end
end

local function save_data()
    if blocked_save or not (smudge and smudge.write_file) then return end
    local el = get_elapsed_ms()
    local lines = { string.format("%d;%d", is_running and 1 or 0, el) }
    for _, sp in ipairs(splits) do
        table.insert(lines, string.format("%d;%d", sp.lap_ms, sp.total_ms))
    end
    local data = table.concat(lines, "\n")
    if #data > MAX_SAVE_BYTES then notice = "Saved data preserved";blocked_save = true;return end
    smudge.write_file("stopwatch.dat", data)
end

local function load_data()
    if not (smudge and smudge.read_file) then return end
    -- One extra byte distinguishes a complete legacy file from a capped prefix.
    local content = smudge.read_file("stopwatch.dat", MAX_SAVE_BYTES + 1)
    if not content or content == "" then return end
    local function invalid()blocked_save = true;notice = "Saved data preserved" end
    if #content > MAX_SAVE_BYTES then invalid();return end
    local header = content:match("^[^\r\n]+") or ""
    local running,elapsed = header:match("^([01]);(%d+)$")
    elapsed = tonumber(elapsed)
    local function valid(n)return math.type(n) == "integer" and n >= 0 and n <= MAX_ELAPSED end
    if not running or not valid(elapsed) then invalid();return end
    local restored,previous,bytes = {},0,0
    local first = true
    for line in content:gmatch("[^\r\n]+") do
        if first then first = false else
            local lap,total = line:match("^(%d+);(%d+)$")
            lap,total = tonumber(lap),tonumber(total)
            if #restored >= 4096 or not valid(lap) or not valid(total) or total < previous or total > elapsed or lap ~= total - previous then invalid();return end
            restored[#restored + 1] = {lap_ms=lap,total_ms=total}
            previous = total;bytes = bytes + #line + 1
        end
    end
    splits,last_split_total,lap_bytes = restored,previous,bytes
    accumulated_ms,is_running = elapsed,running == "1"
    start_time_ms = get_now_ms()
    if #splits >= MAX_SPLITS then notice = "Lap list full" end
end

local function trigger_start_stop()
    if blocked_save then return end
    local now = get_now_ms()
    plunger_down = true
    plunger_timer = now + 160

    if not is_running then
        if accumulated_ms == MAX_ELAPSED then notice = "Time limit reached";smudge.request_update();return end
        is_running = true
        start_time_ms = now
    else
        accumulated_ms = get_elapsed_ms()
        is_running = false
        save_data()
    end
    if smudge and smudge.request_update then smudge.request_update() end
end

local function trigger_split()
    local now = get_now_ms()
    local elapsed = get_elapsed_ms()
    if not is_running then
        return
    end

    local lap_time = elapsed - last_split_total
    local bytes = #string.format("%d;%d",lap_time,elapsed) + 1
    if blocked_save or #splits >= MAX_SPLITS or 12 + lap_bytes + bytes > MAX_SAVE_BYTES then
        if not blocked_save then notice = "Lap list full" end
        smudge.request_update();return
    end
    lap_bytes = lap_bytes + bytes
    last_split_total = elapsed
    table.insert(splits, { lap_ms = lap_time, total_ms = elapsed })
    scroll_offset = 0
    split_flash_timer = now + 1200
    save_data()
    if smudge and smudge.request_update then smudge.request_update() end
end

local function trigger_reset()
    if blocked_save then return end
    notice = "";lap_bytes = 0
    is_running = false
    accumulated_ms = 0
    start_time_ms = 0
    last_split_total = 0
    splits = {}
    scroll_offset = 0
    split_flash_timer = 0
    save_data()
    if smudge and smudge.request_update then smudge.request_update() end
end

function on_init()
    if smudge and smudge.prevent_sleep then
        smudge.prevent_sleep(true)
    end
    if smudge and smudge.get_bounds then
        w, h = smudge.get_bounds()
    end
    load_data()
end

function on_exit()
    if smudge and smudge.prevent_sleep then
        smudge.prevent_sleep(false)
    end
    save_data()
end

function on_update()
    local now = get_now_ms()

    -- Plunger mechanical spring return
    if plunger_down and now >= plunger_timer then
        plunger_down = false
        if smudge and smudge.request_update then smudge.request_update() end
    end

    -- Periodic redraw while running (180ms ~ 5.5 fps partial refresh)
    if is_running and (now - last_render_ms) >= 180 then
        last_render_ms = now
        if smudge and smudge.request_update then smudge.request_update() end
    end
end

local function draw_splits_card(x,y,cw,ch)
    U.panel({x=x,y=y,w=cw,h=ch})
    U.text(x+12,y+10,'SPLITS','ui10',true)
    U.text(x+cw-12,y+10,tostring(#splits)..' LAPS','ui10',true,'right')
    U.text(x+12,y+42,'LAP','ui10',true)
    U.text(x+cw//2,y+42,'SPLIT TIME','ui10',true,'center')
    U.text(x+cw-12,y+42,'TOTAL TIME','ui10',true,'right')
    smudge.line(x+12,y+72,x+cw-12,y+72,true)
    local cap=screen_layout().visible_rows
    scroll_offset=math.min(scroll_offset,math.max(0,#splits-cap))
    local last=#splits-scroll_offset
    if #splits==0 then U.text(x+cw//2,y+100,'No splits recorded','ui10',false,'center') end
    for row=0,math.min(cap,last)-1 do
        local i=last-row;local sp=splits[i];local ry=y+82+row*38
        U.text(x+12,ry,string.format('#%02d',i),'ui10',true)
        local font='ui10'
        local lap='+'..format_time(sp.lap_ms);local total=format_time(sp.total_ms)
        if smudge.text_width(lap,font,true)>cw*0.40 or smudge.text_width(total,font,true)>cw*0.40 then font='small' end
        U.text(x+cw//2,ry,lap,font,true,'center')
        U.text(x+cw-12,ry,total,font,true,'right')
    end
    if #splits>cap then U.text(x+cw//2,y+ch-30,'Tap upper / lower table to scroll','small',false,'center') end
end

function on_draw()
    w,h=smudge.get_bounds()
    smudge.clear()
    local elapsed=get_elapsed_ms()
    U.header('Stopwatch',is_running and 'RUNNING' or (elapsed>0 and 'PAUSED' or 'READY'))
    if notice~='' then U.text(14,78,notice,'ui10',true) end
    local l=screen_layout()
    local value=format_time(elapsed)
    P.draw(value,l.timer.x,l.timer.y,l.timer.w,l.timer.h)
    draw_splits_card(l.card_x,l.sy,l.card_w,l.card_h)
    footer=U.footer({'Exit',is_running and 'Stop' or 'Start','Split','Reset'},2,{[2]=not blocked_save,[3]=is_running and not blocked_save,[4]=not blocked_save})
end

function on_button(btn, pressed)
    if not pressed then return end

    if btn == "btn1" or btn == "back" then
        save_data()
        smudge.exit()
    elseif btn == "btn2" or btn == "confirm" then
        trigger_start_stop()
    elseif btn == "btn3" or btn == "left" or btn == "page_back" then
        trigger_split()
    elseif btn == "btn4" or btn == "right" or btn == "page_forward" or btn == "down" then
        trigger_reset()
    elseif btn == "up" then
        local visible = screen_layout().visible_rows
        if #splits > visible then
            scroll_offset = math.min(#splits - visible, scroll_offset + 1)
            if smudge and smudge.request_update then smudge.request_update() end
        end
    end
end

function on_tap(x, y)
    local layout=screen_layout()
    for i,r in ipairs(footer) do
        if U.hit(r,x,y) then on_button(({'back','confirm','left','right'})[i],true);return end
    end
    if y>=layout.footer_y then return end
    if U.hit(layout.timer,x,y) then trigger_start_stop();return end

    -- Tap splits card to scroll
    if x >= layout.card_x and x < layout.card_x + layout.card_w and
       y >= layout.sy and y < layout.sy + layout.card_h then
        local middle = layout.sy + math.floor(layout.card_h / 2)
        if y < middle and #splits > layout.visible_rows then
            scroll_offset = math.min(#splits - layout.visible_rows, scroll_offset + 1)
            if smudge and smudge.request_update then smudge.request_update() end
        elseif y >= middle and scroll_offset > 0 then
            scroll_offset = math.max(0, scroll_offset - 1)
            if smudge and smudge.request_update then smudge.request_update() end
        end
    end
end
