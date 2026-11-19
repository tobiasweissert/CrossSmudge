-- Desk Stand Dashboard
-- Ambient e-ink desk clock, calendar, and overview station

local U = dofile('unified_ui.lua')
local P = dofile('primary_numerals.lua')
local footer = {}
local is_24h = false
local theme_style = 1
local NUM_STYLES = 7
local last_min, last_hour = -1, -1
local needs_full_refresh = false
local buttons_visible = true
local last_interact_ms = 0
local BUTTON_TIMEOUT_MS = 5000

local MONTHS = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"}
local DAYS = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}
local DAY_NAMES = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"}
local SEGS = {119, 36, 93, 109, 46, 107, 123, 37, 127, 111}
local FIRST_WDAY_T = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4}

local function is_leap(yr)
    return (yr % 4 == 0 and yr % 100 ~= 0) or (yr % 400 == 0)
end

local function days_in_mo(yr, mo)
    if mo == 2 then return is_leap(yr) and 29 or 28 end
    if mo == 4 or mo == 6 or mo == 9 or mo == 11 then return 30 end
    return 31
end

local function first_wday(yr, mo)
    local y = (mo < 3) and (yr - 1) or yr
    return ((y + math.floor(y/4) - math.floor(y/100) + math.floor(y/400) + FIRST_WDAY_T[mo] + 1) % 7) + 1
end

local function day_of_yr(yr, mo, dy)
    local d = dy
    for m = 1, mo - 1 do d = d + days_in_mo(yr, m) end
    return d
end

local function save_data()
    if smudge and smudge.write_file then
        smudge.write_file("dashboard.dat", string.format("%d;%d", is_24h and 1 or 0, theme_style))
    end
end

local function load_data()
    if not (smudge and smudge.read_file) then return end
    local s = smudge.read_file("dashboard.dat")
    if not s or s == "" then s = smudge.read_file("dashboard_v1.dat") end
    if s and s ~= "" then
        local p = {}
        for x in string.gmatch(s, "[^;]+") do table.insert(p, x) end
        if #p >= 2 then
            is_24h = (tonumber(p[1]) or 0) == 1
            theme_style = math.max(1, math.min(NUM_STYLES, tonumber(p[2]) or 1))
        end
    end
end

local function draw_digit(x, y, d, dw, dh, th, col)
    local n = tonumber(d) or 0
    local hh, thh = math.floor(dh / 2), math.floor(th / 2)
    if n == 1 then
        local cx = x + math.floor((dw - th) / 2)
        smudge.rect(cx, y, th, dh, true, col)
        smudge.rect(cx - th + 2, y, th - 2, th + 2, true, col)
        smudge.rect(cx - th + 2, y + dh - th + 2, th * 2 - 2, th - 2, true, col)
        return
    end
    local s = SEGS[n + 1] or 0
    if (s & 1) ~= 0 then smudge.rect(x, y, dw, th, true, col) end
    if (s & 2) ~= 0 then smudge.rect(x, y, th, hh + thh, true, col) end
    if (s & 4) ~= 0 then smudge.rect(x + dw - th, y, th, hh + thh, true, col) end
    if (s & 8) ~= 0 then smudge.rect(x, y + hh - thh, dw, th, true, col) end
    if (s & 16) ~= 0 then smudge.rect(x, y + hh - thh, th, dh - hh + thh, true, col) end
    if (s & 32) ~= 0 then smudge.rect(x + dw - th, y + hh - thh, th, dh - hh + thh, true, col) end
    if (s & 64) ~= 0 then smudge.rect(x, y + dh - th, dw, th, true, col) end
end

function on_init()
    if smudge.prevent_sleep then smudge.prevent_sleep(true) end
    load_data()
    if smudge.set_orientation then smudge.set_orientation(theme_style>=6 and 'landscape' or 'portrait') end
    local dt = smudge.get_date and smudge.get_date()
    if dt then last_min, last_hour = dt.min, dt.hour end
    last_interact_ms = (smudge.millis and smudge.millis()) or (os.time() * 1000)
    buttons_visible = true
end

function on_exit()
    if smudge.prevent_sleep then smudge.prevent_sleep(false) end
    if smudge.set_orientation then smudge.set_orientation("portrait") end
    save_data()
end

function on_update()
    local now = (smudge.millis and smudge.millis()) or (os.time() * 1000)
    if buttons_visible and (now - last_interact_ms >= BUTTON_TIMEOUT_MS) then
        buttons_visible = false
        if smudge.request_update then smudge.request_update() end
    end

    local dt = smudge.get_date and smudge.get_date()
    if not dt then return end
    if dt.min ~= last_min then
        last_min = dt.min
        if dt.hour ~= last_hour then
            last_hour = dt.hour
            needs_full_refresh = true
        end
        if smudge.request_update then smudge.request_update() end
    end
end

function on_draw()
    smudge.clear()
    local m=U.metrics();local w,h=m.w,m.h
    local dt=(smudge.get_date and smudge.get_date()) or os.date('*t')
    local doy=day_of_yr(dt.year,dt.month,dt.day)
    local total=is_leap(dt.year) and 366 or 365
    local day_pct=math.floor((dt.hour*60+dt.min)/1440*100)
    local hour=dt.hour;local ampm=''
    if not is_24h then hour=hour%12;if hour==0 then hour=12 end;ampm=dt.hour>=12 and 'PM' or 'AM' end
    local status=string.format('%s, %s %d',DAYS[dt.wday]:sub(1,3):upper(),MONTHS[dt.month]:sub(1,3):upper(),dt.day)
    U.header('Desk Stand',status)
    local land=w>h
    local clock_x=land and 14 or 14;local clock_w=land and 330 or w-28
    local captioned=theme_style==2 or theme_style==5
    local clock_y=captioned and 96 or 80
    local clock=string.format('%02d:%02d',hour,dt.min)
    local clock_h=captioned and 112 or 128
    local ampm_w=ampm~='' and 48 or 0
    P.draw(clock,clock_x,clock_y,clock_w-ampm_w,clock_h)
    U.text(clock_x+clock_w-4,clock_y+clock_h-56,ampm,'ui12',true,'right')
    if theme_style==2 then
        U.text(clock_x+clock_w//2,clock_y-14,'DIGITAL','small',false,'center')
    end
    if theme_style==5 then
        U.text(clock_x+clock_w//2,land and 280 or 78,'MINIMAL STUDIO','small',false,'center')
    end
    local py=land and 206 or 210
    U.text(clock_x,py,'DAY PROGRESS','ui10')
    U.text(clock_x+clock_w,py,tostring(day_pct)..'%','ui10',false,'right')
    U.panel({x=clock_x,y=py+30,w=clock_w,h=16})
    local fill=math.floor((clock_w-8)*day_pct/100)
    if fill>0 then smudge.rect(clock_x+4,py+34,fill,8,true,true) end
    local stats_y=buttons_visible and h-160 or h-104
    local cal={x=land and 366 or 14,y=land and 78 or 270,w=land and w-380 or w-28,h=stats_y-(land and 78 or 270)-12}
    U.panel(cal)
    U.text(cal.x+cal.w//2,cal.y+8,MONTHS[dt.month]:upper()..' '..dt.year,'ui12',true,'center')
    local col=(cal.w-16)//7
    local row_h=math.min(40,(cal.h-78)//6)
    for i,name in ipairs(DAY_NAMES)do U.text(cal.x+8+(i-1)*col+col//2,cal.y+42,name,'ui10',true,'center') end
    local start=first_wday(dt.year,dt.month)-1
    for d=1,days_in_mo(dt.year,dt.month) do
        local pos=start+d-1;local x=cal.x+8+(pos%7)*col;local y=cal.y+72+(pos//7)*row_h
        local selected=d==dt.day
        if selected then smudge.rect(x,y,col-2,row_h,true,true) end
        U.text(x+(col-2)//2,y+(row_h-25)//2,tostring(d),'ui10',true,'center',not selected)
    end
    local sw=(w-28-16)//3
    local week=math.floor((doy-1)/7)+1
    local cards={{'YEAR PROGRESS',math.floor(doy/total*100)..'%',string.format('DAY %d / %d',doy,total)},
        {'WEEK','W'..week,string.format('%d OF 52',week)},
        {'COUNTDOWN',tostring(total-doy),'DAYS LEFT'}}
    if theme_style==3 then
        local mo_days=days_in_mo(dt.year,dt.month)
        cards[1]={'MONTH PROGRESS',math.floor(dt.day/mo_days*100)..'%',string.format('DAY %d / %d',dt.day,mo_days)}
        cards[3]={'MONTH LEFT',tostring(mo_days-dt.day),'DAYS LEFT'}
    end
    for i,c in ipairs(cards)do
        local x=14+(i-1)*(sw+8)
        U.panel({x=x,y=stats_y,w=sw,h=92})
        U.text(x+sw//2,stats_y+8,U.fit(c[1],sw-12,'small'),'small',false,'center')
        U.text(x+sw//2,stats_y+30,c[2],'ui12',true,'center')
        U.text(x+sw//2,stats_y+62,c[3],'small',false,'center')
    end
    -- Retain all seven saved style choices, with their original display roles.
    -- Dark styles invert only the body; black unified chrome remains constant.
    if theme_style==4 or theme_style==7 then
        -- Native inversion is whole-screen only; dark variants use a filled clock panel.
        smudge.rect(clock_x,clock_y,clock_w,clock_h,true,true)
        P.draw(clock,clock_x,clock_y,clock_w-ampm_w,clock_h,true)
        U.text(clock_x+clock_w-4,clock_y+clock_h-56,ampm,'ui12',true,'right',false)
    end
    footer={}
    if buttons_visible then
        footer=U.footer({'Exit',is_24h and '12-Hour' or '24-Hour','Style -','Style +'},nil)
    else
        -- Header status remains the date; a visible reveal affordance has no fixed footer.
        U.text(clock_x+clock_w//2,land and 312 or 184,'Tap to show controls','small',false,'center',not (not land and (theme_style==4 or theme_style==7)))
    end
    if needs_full_refresh then needs_full_refresh=false;if smudge.full_refresh then smudge.full_refresh() end end
end

function on_button(btn, pressed)
    if not pressed then return end
    last_interact_ms = (smudge.millis and smudge.millis()) or (os.time() * 1000)

    if btn == "btn1" or btn == "back" then
        if smudge.prevent_sleep then smudge.prevent_sleep(false) end
        if smudge.set_orientation then smudge.set_orientation("portrait") end
        smudge.exit()
        return
    end

    buttons_visible = true

    if btn == "btn2" or btn == "confirm" then
        is_24h = not is_24h
        save_data()
        if smudge.request_update then smudge.request_update() end
    elseif btn == "btn3" or btn == "left" or btn == "page_back" then
        theme_style = theme_style - 1
        if theme_style < 1 then theme_style = NUM_STYLES end
        save_data()
        if smudge.set_orientation then smudge.set_orientation(theme_style>=6 and 'landscape' or 'portrait') end
        if smudge.request_update then smudge.request_update() end
    elseif btn == "btn4" or btn == "right" or btn == "page_forward" then
        theme_style = theme_style + 1
        if theme_style > NUM_STYLES then theme_style = 1 end
        save_data()
        if smudge.set_orientation then smudge.set_orientation(theme_style>=6 and 'landscape' or 'portrait') end
        if smudge.request_update then smudge.request_update() end
    end
end

function on_touch(action, tx, ty)
    if smudge.get_device and smudge.get_device() == "sticky" then
        action, tx, ty = ty, action, tx
    end
    if action ~= "tap" and action ~= "click" then return end
    last_interact_ms = (smudge.millis and smudge.millis()) or (os.time() * 1000)
    if not buttons_visible then
        buttons_visible = true
        if smudge.request_update then smudge.request_update() end
        return
    end

    local w, h = 480, 800
    if smudge.get_bounds then w, h = smudge.get_bounds() end
    local is_land=w>h
    for i,r in ipairs(footer)do
        if U.hit(r,tx,ty)then on_button(({'btn1','btn2','btn3','btn4'})[i],true);return end
    end
    if ty>=h-60 then return end

    if is_land then
        on_button(tx < 400 and "btn2" or "btn4", true)
    else
        on_button(ty <= 220 and "btn2" or "btn4", true)
    end
end
