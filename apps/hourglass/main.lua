local U = dofile('unified_ui.lua')
local P = dofile('primary_numerals.lua')
local footer = {}
local w, h = 480, 800
if smudge.get_bounds then
    w, h = smudge.get_bounds()
end

-- Increments and configuration
local STEP_OPTIONS = {
    5, 10, 15, 20, 30, 45, 60, 90, 120, 180, 300, 600, 900, 1200, 1800, 3600
}

local step_top_sec = 300 -- default 5 min
local step_bot_sec = 60  -- default 1 min
local total_sec = 15 * 60 -- default 15 min
local remaining_sec = total_sec

local has_started = false
local is_running = false
local is_alarm = false
local flash_state = false
local flash_timer = 0
local last_flash_ms = 0
local last_tick_ms = 0
local last_rendered_sec = -1
local anim_tick = 0

local in_settings = false
local setting_row = 1 -- 1: Top/Bottom, 2: Bottom -/+
local confirm_held_start = nil
local confirm_suppress_release = false

local banner_msg = ""
local banner_expires = 0

local function show_banner(msg, ms)
    banner_msg = msg
    local now = (smudge.millis and smudge.millis()) or (os.time() * 1000)
    banner_expires = now + (ms or 1500)
end

local function format_step(sec)
    if sec < 60 then
        return string.format("%d sec", sec)
    elseif sec < 3600 then
        local m = math.floor(sec / 60)
        local s = sec % 60
        if s > 0 then
            return string.format("%dm %ds", m, s)
        else
            return string.format("%d min", m)
        end
    else
        local h = math.floor(sec / 3600)
        local m = math.floor((sec % 3600) / 60)
        if m > 0 then
            return string.format("%dh %dm", h, m)
        else
            return string.format("%d hr", h)
        end
    end
end

local function format_time(sec)
    local s = math.max(0, math.floor(sec))
    local h = math.floor(s / 3600)
    local m = math.floor((s % 3600) / 60)
    local rem_s = s % 60
    if h > 0 then
        return string.format("%02d:%02d:%02d", h, m, rem_s)
    else
        return string.format("%02d:%02d", m, rem_s)
    end
end

local function save_data()
    smudge.write_file("hourglass.dat", string.format("%d;%d;%d",
        math.floor(total_sec), math.floor(step_top_sec), math.floor(step_bot_sec)))
end

local function load_data()
    local s = smudge.read_file("hourglass.dat")
    if s and s ~= "" then
        local p = {}
        for x in string.gmatch(s, "[^;]+") do table.insert(p, x) end
        if #p >= 3 then
            local saved_total = tonumber(p[1]) or (15 * 60)
            local saved_top = tonumber(p[2]) or 300
            local saved_bot = tonumber(p[3]) or 60
            if saved_total and saved_total >= 1 then total_sec = saved_total end
            if saved_top and saved_top >= 1 then step_top_sec = saved_top end
            if saved_bot and saved_bot >= 1 then step_bot_sec = saved_bot end
            remaining_sec = total_sec
        end
    end
end

function on_init()
    if smudge and smudge.prevent_sleep then
        smudge.prevent_sleep(true)
    end
    if smudge.get_bounds then
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

local function step_index(current_sec)
    local best_i = 1
    local min_diff = math.abs(STEP_OPTIONS[1] - current_sec)
    for i = 2, #STEP_OPTIONS do
        local diff = math.abs(STEP_OPTIONS[i] - current_sec)
        if diff < min_diff then
            min_diff = diff
            best_i = i
        end
    end
    return best_i
end

local function adjust_step(row, dir)
    local cur = (row == 1) and step_top_sec or step_bot_sec
    local idx = step_index(cur)
    idx = math.max(1, math.min(#STEP_OPTIONS, idx + dir))
    if row == 1 then
        step_top_sec = STEP_OPTIONS[idx]
    else
        step_bot_sec = STEP_OPTIONS[idx]
    end
    save_data()
    if smudge.request_update then smudge.request_update() end
end

local function dismiss_alarm()
    is_alarm = false
    is_running = false
    has_started = false
    remaining_sec = total_sec
    flash_state = false
    show_banner("Ready", 1200)
    if smudge.request_update then smudge.request_update() end
end

local function reset_timer()
    is_running = false
    has_started = false
    remaining_sec = total_sec
    last_rendered_sec = -1
    show_banner("Timer Reset", 1200)
    if smudge.request_update then smudge.request_update() end
end

local function toggle_start_pause()
    if is_alarm then
        dismiss_alarm()
        return
    end
    if not has_started then
        has_started = true
        is_running = true
        last_tick_ms = (smudge.millis and smudge.millis()) or (os.time() * 1000)
        show_banner("Timer Started", 1200)
    else
        is_running = not is_running
        if is_running then
            last_tick_ms = (smudge.millis and smudge.millis()) or (os.time() * 1000)
            show_banner("Timer Resumed", 1200)
        else
            show_banner("Timer Paused", 1200)
        end
    end
    if smudge.request_update then smudge.request_update() end
end

local function adjust_duration(amount)
    if is_running or has_started or is_alarm then return end
    total_sec = math.max(5, math.min(86400, total_sec + amount))
    remaining_sec = total_sec
    last_rendered_sec = -1
    save_data()
    if smudge.request_update then smudge.request_update() end
end

function on_update()
    anim_tick = anim_tick + 1
    local now = (smudge.millis and smudge.millis()) or (os.time() * 1000)

    -- Auto-clear banner
    if banner_msg ~= "" and banner_expires > 0 then
        if now >= banner_expires then
            banner_msg = ""
            banner_expires = 0
            if smudge.request_update then smudge.request_update() end
        end
    end

    -- Hold Start for Settings
    if smudge.is_button_down and (smudge.is_button_down("confirm") or smudge.is_button_down("btn2")) then
        if not confirm_held_start and not in_settings and not is_alarm then
            confirm_held_start = now
        elseif confirm_held_start and (now - confirm_held_start) >= 500 and not in_settings and not is_alarm then
            in_settings = true
            confirm_suppress_release = true
            confirm_held_start = nil
            if smudge.request_update then smudge.request_update() end
        end
    else
        confirm_held_start = nil
    end

    -- Alarm Flashing state (toggles screen inversion every 500ms)
    if is_alarm then
        if not last_flash_ms then last_flash_ms = now end
        if (now - last_flash_ms) >= 500 then
            last_flash_ms = now
            flash_state = not flash_state
            if smudge.request_update then smudge.request_update() end
        end
        return
    end

    -- Timer countdown execution
    if is_running then
        local delta_ms = now - last_tick_ms
        if delta_ms >= 200 then
            last_tick_ms = now
            remaining_sec = math.max(0, remaining_sec - (delta_ms / 1000.0))

            if remaining_sec <= 0 then
                remaining_sec = 0
                is_running = false
                is_alarm = true
                last_flash_ms = now
                flash_state = true
                show_banner("TIME'S UP!", 5000)
                if smudge.full_refresh then smudge.full_refresh() end
                if smudge.request_update then smudge.request_update() end
            else
                local cur_sec = math.floor(remaining_sec)
                if cur_sec ~= last_rendered_sec then
                    last_rendered_sec = cur_sec
                    if smudge.request_update then smudge.request_update() end
                end
            end
        end
    end
end

-- ----------------------------------------------------------------------------
-- Drawing Routines
-- ----------------------------------------------------------------------------

local function draw_hourglass(cx,cy,pct)
    local ah=w>h and 200 or 250
    local aw=180;local half=ah//2
    smudge.rect(cx-aw//2,cy,aw,12,true,true)
    smudge.rect(cx-aw//2,cy+ah,aw,12,true,true)
    for _,side in ipairs({-1,1}) do
        smudge.thick_line(cx+side*(aw//2-16),cy+12,cx+side*7,cy+half,5,true)
        smudge.thick_line(cx+side*7,cy+half,cx+side*(aw//2-16),cy+ah,5,true)
    end
    -- State-driven monochrome sand; no cartoon face or decorative casing.
    local top=math.floor((half-18)*(1-pct))
    for dy=18+top,half-8,3 do
        local radius=math.max(1,math.floor((half-dy)*0.5))
        smudge.line(cx-radius,cy+dy,cx+radius,cy+dy,true)
    end
    local bottom=math.floor((half-18)*(1-pct))
    for dy=ah-bottom,ah-8,3 do
        local radius=math.max(1,math.floor((dy-half)*0.5))
        smudge.line(cx-radius,cy+dy,cx+radius,cy+dy,true)
    end
    if is_running then smudge.line(cx,cy+half,cx,cy+ah-12,true) end
end

local function settings_geometry()
    local width, height = smudge.get_bounds()
    local mw, mh = math.min(430, width - 32), 300
    return math.floor((width - mw) / 2), math.max(60, math.floor((height - mh) / 2)), mw, mh
end

-- Shared half-open rectangles for both paint and Sticky dispatch.
local function settings_rect(row)
    local mx, my, mw = settings_geometry()
    if row == 3 then return mx + 20, my + 244, mw - 40, 40 end
    return w // 2 - 130, my + (row == 1 and 80 or 164), 260, 36
end
local function contains(x, y, rx, ry, rw, rh)
    return x >= rx and x < rx + rw and y >= ry and y < ry + rh
end

local function screen_layout()
    local land=w>h
    return {ax=land and w//4 or w//2,ay=land and 128 or 186,ah=land and 200 or 250,
        x=land and w//2+14 or 14,cw=land and w//2-28 or w-28,
        cx=land and 3*w//4 or w//2,time_y=land and 150 or 488,
        card_y=land and 224 or 574,prompt_y=land and 350 or 670}
end

local function draw_settings_dialog(cx,w,h)
    local mx,my,mw,mh=settings_geometry()
    U.panel({x=mx,y=my,w=mw,h=mh})
    U.text(cx,my+12,'Adjust Increments','ui12',true,'center')
    U.text(cx,my+48,'Top / Bottom: coarse step','ui10',false,'center')
    U.text(cx,my+132,'Screen - / +: fine step','ui10',false,'center')
    for row=1,3 do
        local x,y,rw,rh=settings_rect(row)
        local label=row==3 and 'Done' or '<  '..format_step(row==1 and step_top_sec or step_bot_sec)..'  >'
        U.button(label,{x=x,y=y,w=rw,h=rh},row==setting_row)
    end
    U.text(cx,my+212,'Tap left / right to adjust','ui10',false,'center')
end

function on_draw()
    w,h=smudge.get_bounds();smudge.clear()
    U.header('Hourglass',is_alarm and "TIME'S UP!" or (is_running and 'RUNNING' or (has_started and 'PAUSED' or 'READY')))
    if in_settings then
        draw_settings_dialog(w//2,w,h)
        footer=U.footer({'Done','Select','-','+'},1)
        if is_alarm and flash_state and smudge.invert_screen then smudge.invert_screen() end
        return
    end
    if banner_msg~='' then U.text(w//2,78,U.fit(banner_msg,w-28,'ui10',true),'ui10',true,'center') end
    local pct=total_sec>0 and math.max(0,math.min(1,remaining_sec/total_sec)) or 0
    local l=screen_layout();draw_hourglass(l.ax,l.ay,pct)
    P.draw(format_time(remaining_sec),l.x,l.time_y-8,l.cw,w>h and 76 or 96)
    U.panel({x=l.x,y=l.card_y,w=l.cw,h=14})
    local fill=math.floor((l.cw-8)*(1-pct))
    if fill>0 then smudge.rect(l.x+4,l.card_y+4,fill,6,true,true) end
    U.text(l.cx,l.card_y+30,'PROGRESS: '..math.floor((1-pct)*100)..'% - TOTAL: '..format_time(total_sec),'ui10',true,'center')
    if not has_started then
        U.button('Set increments: coarse / fine',{x=l.x,y=l.card_y+66,w=l.cw,h=48})
    elseif is_alarm then U.text(l.cx,l.card_y+66,'Tap anywhere to dismiss','ui10',true,'center') end
    footer=U.footer({'Exit',is_running and 'Pause' or 'Start',has_started and 'Reset' or '-',has_started and 'Set' or '+'},2)
    if is_alarm and flash_state and smudge.invert_screen then smudge.invert_screen() end
end

function on_button(btn, pressed)
    if not pressed then return end

    -- Dismiss alarm on any button
    if is_alarm then
        dismiss_alarm()
        return
    end

    -- Check if confirm was held (suppress release trigger)
    if (btn == "btn2" or btn == "confirm") and confirm_suppress_release then
        confirm_suppress_release = false
        return
    end

    -- Settings modal interaction
    if in_settings then
        if btn == "btn1" or btn == "back" then
            in_settings = false
            save_data()
            show_banner("Settings Saved", 1200)
            if smudge.request_update then smudge.request_update() end
        elseif btn == "btn2" or btn == "confirm" then
            setting_row = (setting_row == 1) and 2 or 1
            if smudge.request_update then smudge.request_update() end
        elseif btn == "btn3" or btn == "left" then
            adjust_step(setting_row, -1)
        elseif btn == "btn4" or btn == "right" then
            adjust_step(setting_row, 1)
        elseif btn == "up" or btn == "page_back" then
            setting_row = 1
            if smudge.request_update then smudge.request_update() end
        elseif btn == "down" or btn == "page_forward" then
            setting_row = 2
            if smudge.request_update then smudge.request_update() end
        end
        return
    end

    -- Main screen button interaction
    if btn == "btn1" or btn == "back" then
        smudge.exit()
    elseif btn == "btn2" or btn == "confirm" then
        toggle_start_pause()
    elseif btn == "btn3" or btn == "left" then
        if has_started then
            reset_timer()
        else
            adjust_duration(-step_bot_sec)
        end
    elseif btn == "btn4" or btn == "right" then
        if not has_started then
            adjust_duration(step_bot_sec)
        end
    elseif btn == "up" or btn == "page_back" then
        adjust_duration(-step_top_sec)
    elseif btn == "down" or btn == "page_forward" then
        adjust_duration(step_top_sec)
    end
end

function on_touch(action, tx, ty)
    if smudge.get_device and smudge.get_device() == "sticky" then
        action, tx, ty = ty, action, tx
    end
    if action ~= "tap" and action ~= "click" then return end
    local cx = math.floor(w / 2)

    if is_alarm then
        dismiss_alarm()
        return
    end

    if in_settings then
        for i,r in ipairs(footer) do
            if U.hit(r,tx,ty) then on_button(({'back','confirm','left','right'})[i],true);return end
        end
        for row=1,3 do
            if contains(tx,ty,settings_rect(row)) then
                if row==3 then
                    in_settings=false;save_data();show_banner('Settings Saved',1200)
                else
                    setting_row=row;adjust_step(row,tx<cx and -1 or 1)
                end
                if smudge.request_update then smudge.request_update() end
                return
            end
        end
        return
    end

    for i,r in ipairs(footer) do
        if U.hit(r,tx,ty) then
            if i==4 and has_started then
                in_settings=true
                if smudge.request_update then smudge.request_update() end
            else on_button(({'back','confirm','left','right'})[i],true) end
            return
        end
    end
    if ty>=h-60 then return end
    local layout=screen_layout()
    if contains(tx,ty,layout.ax-90,layout.ay,180,layout.ah+12)
        or contains(tx,ty,layout.x,layout.time_y,layout.cw,48) then
        toggle_start_pause()
    elseif not has_started and contains(tx,ty,layout.x,layout.card_y+66,layout.cw,48) then
        in_settings=true
        if smudge.request_update then smudge.request_update() end
    end
end

-- A changed view is not an input target until its complete draw returns.
-- This also catches hold-to-settings and countdown-to-alarm transitions.
local painted_view
local function input_view()
    return tostring(in_settings)..':'..tostring(is_alarm)..':'..tostring(has_started)
        ..':'..tostring(is_running)..':'..setting_row
end
local function sticky_input_ready()
    return not (smudge.get_device and smudge.get_device() == 'sticky')
        or painted_view == input_view()
end
local function retire_changed_view()
    if not sticky_input_ready() then footer = {} end
end
local draw_view, button_view, touch_view, update_view = on_draw, on_button, on_touch, on_update
function on_draw()
    painted_view = nil
    draw_view()
    painted_view = input_view()
end
function on_button(btn, pressed)
    if not sticky_input_ready() then return end
    button_view(btn, pressed)
    retire_changed_view()
end
function on_touch(action, tx, ty)
    if not sticky_input_ready() then return end
    touch_view(action, tx, ty)
    retire_changed_view()
end
function on_update()
    update_view()
    retire_changed_view()
end
