local screen = {}
local evId = 0
local choiceIdx = 1
local ev = nil

local function get_event(id)
    id = id or 0
    local res = { id = id, narrative = {}, choices = {} }
    if smudge and smudge.find_section then
        local n = 0
        smudge.find_section("events.txt", "[" .. tostring(id) .. "]", "[", function(line)
            n = n + 1
            if n == 1 then
                local t, s = line:match("^([^|]+)|(.*)$")
                res.title, res.sub = t or "Scriptorium", s or "Sanctuary"
            elseif n == 2 then
                for p in line:gmatch("[^|]+") do res.narrative[#res.narrative + 1] = p end
            else
                local c = {}
                for p in line:gmatch("[^|]+") do c[#c + 1] = tonumber(p) or p end
                res.choices[#res.choices + 1] = c
            end
            return true
        end)
    end
    return res
end

function screen.enter(arg)
    if arg and arg.newEvent then state.eventId = math.random(0, 8) end
    evId = state.eventId or 0
    ev = get_event(evId)
    choiceIdx = 1
    state.savedScreen = "scriptorium"
    save_game_state()
    collectgarbage("collect")
end

function screen.exit()
    ev = nil
    collectgarbage("collect")
end

function screen.draw()
    local m=U.metrics();local cur=ev or get_event(evId)
    U.header(cur.title,cur.sub)
    U.text(14,80,'HP '..state.playerHp..'/'..state.playerMaxHp..'  GOLD '..state.gold,'ui10',true)
    draw_miniature_frame(14,114,136,136)
    smudge.draw_sprite(18,118,128,128,'sprites/event_'..cur.id..'.raw')
    local nx,ny,nw=160,114,m.landscape and 260 or w-174
    for _,p in ipairs(cur.narrative)do
        local res=smudge.wrapped_text(p,nw,'small',false,12)
        for _,line in ipairs(res and res.lines or {})do U.text(nx,ny,line,'small');ny=ny+smudge.line_height('small')end
    end
    screen.options={}
    local x,y,cw=m.landscape and 440 or 14,m.landscape and 114 or math.max(266,ny+12),m.landscape and w-454 or w-28
    local first=m.landscape and choiceIdx or 1
    local last=m.landscape and choiceIdx or #cur.choices
    for i=first,last do
        local c=cur.choices[i];local r={x=x,y=y+(i-first)*90,w=cw,h=82};screen.options[i]=r
        local selected=i==choiceIdx
        U.panel(r,selected)
        U.text(x+10,r.y+8,U.fit(c[1],cw-20,'ui10',true),'ui10',true,'left',not selected)
        local text=(c[5]<0 and state.gold < -c[5])and ('Requires '..(-c[5])..' Gold')or c[2]
        local dy=r.y+38
        for _,line in ipairs(smudge.wrapped_text(text,cw-20,'small',false,2).lines or {})do U.text(x+10,dy,line,'small',false,'left',not selected);dy=dy+smudge.line_height('small')end
    end
    draw_actions('','Select','<','>')
end

function screen.on_button(btn, pressed)
    if not pressed then return end
    if btn == "up" or btn == "page_back" then
        set_screen("deck_view", {tab = 1, return_screen = "scriptorium"})
        return
    elseif btn == "down" or btn == "page_forward" then
        set_screen("deck_view", {tab = 4, return_screen = "scriptorium"})
        return
    end

    local cur = ev or get_event(evId)
    if btn == "confirm" then
        local c = cur.choices[choiceIdx]
        if c[5] < 0 and state.gold < -c[5] then return end
        set_screen("outcome", { choice = c })
    elseif btn == "left" then
        choiceIdx = choiceIdx > 1 and (choiceIdx - 1) or #cur.choices
        smudge.request_update()
    elseif btn == "right" then
        choiceIdx = choiceIdx < #cur.choices and (choiceIdx + 1) or 1
        smudge.request_update()
    end
end

function screen.on_tap(x,y)
    for i,r in pairs(screen.options or {})do if U.hit(r,x,y)then choiceIdx=i;screen.on_button('confirm',true);return end end
end

return screen
