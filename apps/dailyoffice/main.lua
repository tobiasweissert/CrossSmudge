-- Divine Worship: Daily Office application for CrossSmudge (Lua)
-- High-efficiency liturgical prayer book designed for 75 KB embedded DRAM limits.
-- Streams all compiled pages directly into cache_hour.txt so active office memory remains flat (~55-60 KB).

local H = {
    {"Mattins", "Morning Prayer", "hours/mattins.txt"},
    {"Prime", "First Hour", "hours/prime.txt"},
    {"Terce", "Third Hour", "hours/terce.txt"},
    {"Sext", "Sixth Hour", "hours/sext.txt"},
    {"None", "Ninth Hour", "hours/none.txt"},
    {"Evensong", "Evening Prayer", "hours/evensong.txt"},
    {"Compline", "Night Prayer", "hours/compline.txt"},
}

local hymnPaths = {"hours/hymns.txt", "hymns.txt", "/.crosssmudge/applications/dailyoffice/hours/hymns.txt"}
local psalterPaths = {"psalter.txt", "hours/psalter.txt", "/.crosssmudge/applications/dailyoffice/psalter.txt"}
local propersPaths = {"propers.txt", "/.crosssmudge/applications/dailyoffice/propers.txt"}

local U = dofile('unified_ui.lua')
local rows, footer = {}, {}
local painted_state = nil
local state, sel, cur_name = "menu", 1, ""
local total_pages, cur_page = 0, 1
local w, h = 480, 800
local banner_str = ""
local feast_title = "Ordinary Time"
local feast_color = "Green"
local date_key = "2026-10-03"
local day_num = 3

local mp_pss, ep_pss = "", ""
local mp_l1, ep_l1 = "", ""
local mp_l2, ep_l2 = "", ""
local collect_txt = ""

local function stream_sec(paths, tag, endP, cb)
    if type(paths) == "string" then paths = {paths} end
    for _, p in ipairs(paths) do
        if smudge.find_section and smudge.find_section(p, tag, endP, cb) then return true end
    end
    return false
end

local function parse_psalms(str)
    local res = {}
    if not str or #str == 0 then return res end
    if str:find("119:") then table.insert(res, str); return res end
    for token in str:gmatch("[^,%s]+") do
        local s, e = token:match("^(%d+)%-(%d+)$")
        if s and e then
            for i = tonumber(s), tonumber(e) do table.insert(res, tostring(i)) end
        else
            table.insert(res, token)
        end
    end
    return res
end

local function load_propers()
    local d = (smudge.get_date and smudge.get_date()) or os.date("*t")
    if not d or not d.year or d.year < 2024 then d = {year=2026, month=10, day=3, wday=7} end
    day_num = d.day or 3
    date_key = string.format("%04d-%02d-%02d", d.year, d.month, d.day)
    local days = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"}
    local mos = {"January","February","March","April","May","June","July","August","September","October","November","December"}
    banner_str = string.format("%s, %s %d, %04d", days[d.wday] or "Saturday", mos[d.month] or "October", d.day, d.year)

    if smudge.find_section then
        for _, p in ipairs(propersPaths) do
            local found = false
            local continued = {}
            smudge.find_section(p, "[" .. date_key .. "]", "[", function(rawL)
                found = true
                local k, plus, v = rawL:gsub("^%s+", ""):gsub("%s+$", ""):match("^([%w_]+)(%+?)=(.*)$")
                if k and v then
                    k = k:upper()
                    v = v:gsub("^%s+", ""):gsub("%s+$", "")
                    if plus == "+" then v = (continued[k] or "") .. " " .. v end
                    continued[k] = v
                    if k == "TITLE" then feast_title = v
                    elseif k == "COLOR" then feast_color = v
                    elseif k == "MP_PSS" then mp_pss = v
                    elseif k == "EP_PSS" then ep_pss = v
                    elseif k == "MP_L1" then mp_l1 = v
                    elseif k == "EP_L1" then ep_l1 = v
                    elseif k == "MP_L2" then mp_l2 = v
                    elseif k == "EP_L2" then ep_l2 = v
                    elseif k == "COLLECT" then collect_txt = v end
                end
            end)
            if found then break end
        end
    end
end

function on_init()
    w, h = smudge.get_bounds()
    state = "menu"
    sel = 1
    load_propers()
    collectgarbage("collect")
end

local function draw_menu()
    local m=U.metrics()
    U.header('Daily Office','DIVINE WORSHIP')
    U.text(14,m.body_top,U.fit(banner_str:upper(),w-28,'ui10',true),'ui10',true)
    local proper=(feast_title..' ('..feast_color..')'):upper()
    local wrapped=smudge.wrapped_text(proper,w-28,'small','regular',100)
    local lines=(wrapped and wrapped.lines) or wrapped or {proper}
    local count=math.max(1,math.min(3,#lines))
    for i=1,count do
        local text=lines[i] or ''
        if i==3 and #lines>3 then text='Open an hour for the full proper' end
        U.text(14,m.body_top+30+(i-1)*smudge.line_height('small'),text,'small')
    end
    local top=m.body_top+30+count*smudge.line_height('small')+8
    local height=math.min(58,(m.body_bottom-top-6*8)//7)
    rows={}
    for i,it in ipairs(H)do
        local r={x=14,y=top+(i-1)*(height+8),w=w-28,h=height}
        rows[i]=r
        local selected=i==sel
        U.panel(r,selected)
        if height<48 then
            U.text(28,r.y+(height-25)//2,it[1]:upper(),'ui10',true,'left',not selected)
            U.text(w-28,r.y+(height-20)//2,it[2]:upper(),'small',false,'right',not selected)
        else
            U.text(28,r.y+4,it[1]:upper(),'ui10',true,'left',not selected)
            U.text(28,r.y+30,it[2]:upper(),'small',false,'left',not selected)
        end
    end
    footer=U.footer({'Back','Open'},2)
end

local function draw_reader()
    local m = U.metrics()
    U.header(cur_name, string.format("%d / %d", cur_page, total_pages))
    local cx, cw = 14, w - 28
    local y = m.body_top
    local lh = smudge.line_height("ui12") + 4
    local rh = smudge.line_height("small") + 2
    local hh = smudge.line_height("ui12") + 8

    if smudge.find_section then
        smudge.find_section("cache_hour.txt", string.format("[PAGE %d]", cur_page), "[PAGE ", function(rawL)
            local l = rawL:gsub("^%s+", ""):gsub("%s+$", "")
            if #l == 0 or l == " " then
                y = y + math.floor(lh / 2)
                return
            end
            local p = ""
            local t = l
            if #l >= 2 and l:sub(2, 2) == ":" then
                p = l:sub(1, 2)
                t = l:sub(3)
            end
            if p == "d:" then
                local mid = cx + math.floor(cw / 2)
                smudge.line(mid - 35, y + 6, mid + 35, y + 6)
                y = y + 14
            elseif p == "h:" then
                smudge.centered_text(y, t, "ui12", "bold", true)
                y = y + hh
            elseif p == "r:" then
                smudge.text(cx + 14, y, t, "small", "regular", "left", true)
                y = y + rh
            elseif p == "v:" or p == "R:" then
                smudge.text(cx, y, p:sub(1, 1) .. ":", "ui12", "bold", "left", true)
                smudge.text(cx + 30, y, t, "ui12", "regular", "left", true)
                y = y + lh
            elseif p == "i:" or p == "j:" then
                smudge.text(cx + 30, y, t, "ui12", "regular", "left", true)
                y = y + lh
            else
                smudge.text(cx, y, t, "ui12", "regular", "left", true)
                y = y + lh
            end
        end)
    end
    footer=U.footer({'Back','Prev',(cur_page<total_pages) and 'Next' or 'Done'},3,{[2]=cur_page>1})
end

local build_hour
function on_draw()
    smudge.clear()
    local nw,nh=smudge.get_bounds()
    if nw~=w or nh~=h then
        w,h=nw,nh
        if state=='reader' then
            build_hour(sel,H[sel][3])
            cur_page=math.max(1,math.min(cur_page,total_pages))
        end
    end
    if state == "menu" then draw_menu() else draw_reader() end
    painted_state=state
end

build_hour = function(hIdx, hFile)
    total_pages = 0
    collectgarbage("collect")
    local curY = 0
    local m = U.metrics()
    local cw = w - 28
    local lh = smudge.line_height("ui12") + 4
    local rh = smudge.line_height("small") + 2
    local hh = smudge.line_height("ui12") + 8
    local availH = m.body_bottom - m.body_top

    local function emit(str)
        local cost = lh
        local p = str:sub(1, 2)
        if p == "d:" then cost = 14
        elseif p == "h:" then cost = hh
        elseif p == "r:" then cost = rh
        elseif p == "  " or str == " " then cost = math.floor(lh / 2) end

        if curY + cost > availH and curY > 0 then
            total_pages = total_pages + 1
            curY = 0
        end
        if curY == 0 then
            smudge.write_file("cache_hour.txt", string.format("[PAGE %d]\n", total_pages + 1), total_pages > 0)
        end
        smudge.write_file("cache_hour.txt", str .. "\n", true)
        curY = curY + cost
    end

    local function blank() emit(" ") end
    local function divider() emit("d:") end
    local function wrap(p1, p2, text, maxW, font, style)
        if not text or #text == 0 then return end
        if not text:find("\n") and smudge.text_width(text,font or 'ui12',style or 'regular') <= (maxW or cw) then emit(p1 .. text); return end
        local res = smudge.wrapped_text(text, maxW or cw, font or "ui12", style or "regular", 100)
        local lns = (res and res.lines) or res or {}
        for i, l in ipairs(lns) do emit(((i == 1) and p1 or p2) .. l) end
    end

    local function stream_hymn(tag)
        local v, r = nil, nil
        divider()
        stream_sec(hymnPaths, "[" .. tag .. "]", "[", function(rawL)
            local l = rawL:gsub("^%s+", ""):gsub("%s+$", "")
            if l:sub(1, 6) == "TITLE=" then
                wrap("h:", "h:", "=== HYMN: " .. l:sub(7) .. " ===", cw - 20, "ui12", "bold")
                blank()
            elseif l:sub(1, 2) == "V=" then v = l:sub(3)
            elseif l:sub(1, 2) == "R=" then r = l:sub(3)
            elseif #l > 0 then wrap("b:", "b:", l, cw)
            else blank() end
        end)
        blank()
        if v and r then
            wrap("v:", "i:", v, cw - 32)
            wrap("R:", "j:", r, cw - 32)
            blank()
        end
        divider()
    end

    local function stream_antiphon(key)
        stream_sec(hymnPaths, "[ANTIPHONS]", "[", function(rawL)
            local k, val = rawL:match("^([%w_]+)=(.*)$")
            if k == key then
                wrap("r:", "r:", "¶ Antiphon: " .. val:gsub("^%s+", ""):gsub("%s+$", ""), cw - 24, "small")
                blank()
            end
        end)
    end

    wrap("h:", "h:", banner_str, cw - 20, "ui12", "bold")
    wrap("r:", "r:", "¶ " .. feast_title .. " (" .. feast_color .. ")", cw - 24, "small")
    divider()

    local invKey = "INV_DEFAULT"
    if feast_title:find("Doctor") or feast_title:find("Confessor") or feast_title:find("Jerome") then invKey = "INV_DOCTOR"
    elseif feast_title:find("Easter") then invKey = "INV_EASTER"
    elseif feast_title:find("Lent") then invKey = "INV_LENT"
    elseif feast_title:find("Advent") then invKey = "INV_ADVENT" end

    local benKey = "BEN_DEFAULT"
    if feast_title:find("Doctor") or feast_title:find("Jerome") then benKey = "BEN_DOCTOR"
    elseif feast_title:find("Martyr") then benKey = "BEN_MARTYR"
    elseif feast_title:find("Apostle") then benKey = "BEN_APOSTLE"
    elseif feast_title:find("Mary") then benKey = "BEN_MARY"
    elseif feast_title:find("Saint") or feast_title:find("Bishop") or feast_title:find("Confessor") then benKey = "BEN_SAINT" end

    local hymnTag = (hIdx == 1) and "ECCE_JAM_NOCTIS" or "LUCIS_CREATOR_OPTIME"
    if feast_title:find("Doctor") or feast_title:find("Confessor") or feast_title:find("Jerome") then hymnTag = "ISTE_CONFESSOR"
    elseif feast_title:find("Martyr") then hymnTag = "DEUS_TUORUM_MILITUM" end

    local skipP, skipL, skipC = false, false, false

    stream_sec({hFile, "apps/dailyoffice/" .. hFile}, "", "", function(rawL)
        local l = rawL:gsub("^%s+", ""):gsub("%s+$", "")
        if l:sub(1, 20) == "Personal Ordinariate" or l:sub(1, 18) == "=== DIVINE WORSHIP" then return
        elseif l:sub(1, 3) == "===" and l:sub(-3) == "===" then
            local title = l:gsub("^===%s*", ""):gsub("%s*===$", "")
            skipP, skipL, skipC = false, false, false

            if title:find("INVITATORY") and hIdx == 1 then stream_antiphon(invKey)
            elseif title:find("BENEDICTUS") and hIdx == 1 then stream_hymn(hymnTag); stream_antiphon(benKey)
            elseif title:find("MAGNIFICAT") and hIdx == 6 then stream_hymn(hymnTag); stream_antiphon(benKey) end

            divider()
            wrap("h:", "h:", title, cw - 20, "ui12", "bold")
            blank()

            if title:find("PSALMODY") and (hIdx == 1 or hIdx == 6) then
                local pss = (hIdx == 1) and mp_pss or ep_pss
                local tablePss = "1, 2, 3, 4, 5"
                stream_sec(hymnPaths, "[30DAY_PSALTER]", "[", function(psLine)
                    local dayN, val = psLine:match("^(%d+)=(.*)$")
                    if tonumber(dayN) == day_num then
                        local mP, eP = val:match("^([^|]+)|(.*)$")
                        tablePss = (hIdx == 1) and mP or eP
                    end
                end)

                local actPss = (not pss or #pss == 0 or feast_title:find("Memorial") or feast_title:find("Feria")) and tablePss or pss
                wrap("r:", "r:", "¶ Appointed Psalms for today: Psalm " .. actPss, cw - 24, "small")
                wrap("r:", "r:", "¶ (30-Day Psalter: Day " .. tostring(day_num) .. ", Psalms " .. tablePss .. ")", cw - 24, "small")
                blank()

                for _, ref in ipairs(parse_psalms(actPss)) do
                    local isFirst = true
                    stream_sec(psalterPaths, "[PSALM " .. ref .. "]", "[", function(rawP)
                        local pL = rawP:gsub("^%s+", ""):gsub("%s+$", "")
                        if #pL > 0 then
                            if isFirst and pL:sub(1, 5) == "Psalm" then
                                isFirst = false; wrap("h:", "h:", pL, cw - 20, "ui12", "bold"); blank()
                            else
                                isFirst = false; wrap("b:", "b:", pL, cw)
                            end
                        else blank() end
                    end)
                    blank()
                    wrap('b:','b:',"Glory be to the Father, and to the Son :",cw)
                    wrap('b:','b:',"and to the Holy Ghost;",cw)
                    wrap('b:','b:',"As it was in the beginning, is now, and ever shall be :",cw)
                    wrap('b:','b:',"world without end. Amen.",cw)
                    blank()
                    divider()
                end
                skipP = true
            elseif (title:find("FIRST LESSON") or title:find("SECOND LESSON")) and (hIdx == 1 or hIdx == 6) then
                local is1 = title:find("FIRST")
                local tag = is1 and ((hIdx == 1) and "MP_L1" or "EP_L1") or ((hIdx == 1) and "MP_L2" or "EP_L2")
                local cit = is1 and ((hIdx == 1) and mp_l1 or ep_l1) or ((hIdx == 1) and mp_l2 or ep_l2)
                local lNm = is1 and "First Lesson" or "Second Lesson"

                if cit and #cit > 0 then wrap("r:", "r:", "¶ Appointed " .. lNm .. ": " .. cit, cw - 24, "small"); blank() end

                local has_text = false
                local loaded = stream_sec({"lessons/" .. date_key .. ".txt", "/.crosssmudge/applications/dailyoffice/lessons/" .. date_key .. ".txt"}, "[" .. tag .. "]", "[", function(rawPara)
                    local para = rawPara:gsub("^%s+", ""):gsub("%s+$", "")
                    if #para > 0 then has_text = true;wrap("b:", "b:", para, cw) else blank() end
                end)
                blank()
                if loaded and has_text then
                    wrap("v:", "i:", "Here endeth the " .. lNm .. ".", cw - 32)
                    wrap("R:", "j:", "Thanks be to God.", cw - 32)
                else
                    wrap("r:", "r:", "Lesson text unavailable: not included for this date. Read the appointed citation separately.", cw - 24, "small")
                end
                blank()
                divider()
                skipL = true
            elseif title:find("COLLECT") then
                if collect_txt and #collect_txt > 0 then
                    wrap("r:", "r:", "¶ Collect of the Day", cw - 24, "small")
                    wrap("b:", "b:", collect_txt, cw)
                    blank()
                    divider()
                    skipC = true
                end
            end
        else
            if not skipP and not skipL and not skipC then
                if l:sub(1, 1) == "¶" then wrap("r:", "r:", l, cw - 24, "small"); blank()
                elseif l:sub(1, 2) == "V." or l:sub(1, 2) == "v." then wrap("v:", "i:", l:sub(3):gsub("^%s+", ""), cw - 32)
                elseif l:sub(1, 2) == "R." or l:sub(1, 2) == "r." then wrap("R:", "j:", l:sub(3):gsub("^%s+", ""), cw - 32); blank()
                elseif #l == 0 then blank()
                else wrap("b:", "b:", l, cw) end
            end
        end
    end)

    if curY > 0 then total_pages = total_pages + 1 end
    collectgarbage("collect")
end

local function open_hour(idx)
    collectgarbage("collect")
    cur_name = H[idx][1]
    build_hour(idx, H[idx][3])
    collectgarbage("collect")
    cur_page = 1
    state = "reader"
end

function on_button(btn, pressed)
    if not pressed then return end
    if state == "menu" then
        if btn == "back" then smudge.exit()
        elseif btn == "up" or btn == "left" or btn == "page_back" then sel = sel - 1; if sel < 1 then sel = #H end; smudge.request_update()
        elseif btn == "down" or btn == "right" or btn == "page_forward" then sel = sel + 1; if sel > #H then sel = 1 end; smudge.request_update()
        elseif btn == "confirm" then open_hour(sel); smudge.request_update() end
    else
        if btn == "back" then total_pages = 0; collectgarbage("collect"); state = "menu"; smudge.request_update()
        elseif btn == "up" or btn == "left" or btn == "page_back" then if cur_page > 1 then cur_page = cur_page - 1; smudge.request_update() end
        elseif btn == "down" or btn == "right" or btn == "page_forward" or btn == "confirm" then
            if cur_page < total_pages then cur_page = cur_page + 1; smudge.request_update()
            else total_pages = 0; collectgarbage("collect"); state = "menu"; smudge.request_update() end
        end
    end
end

function on_tap(x,y)
    local cw,ch=smudge.get_bounds();if cw~=w or ch~=h then return end
    if painted_state~=state then return end
    for i,r in ipairs(footer)do
        if U.hit(r,x,y)then
            if state=='menu' then on_button(i==1 and 'back' or 'confirm',true)
            else on_button(({'back','up','down'})[i],true)end
            return
        end
    end
    local m=U.metrics()
    if x<0 or x>=m.w or y<m.body_top or y>=m.body_bottom then return end
    if state=='menu' then
        for i,r in ipairs(rows)do if U.hit(r,x,y)then sel=i;on_button('confirm',true);return end end
    else on_button(x>w//2 and 'down' or 'up',true)end
end
