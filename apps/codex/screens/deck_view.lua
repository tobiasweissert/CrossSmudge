
local screen = {}
local tab = 1
local page = 0
local menuIdx = 1
local retScreen = 'title'

local capacity=8
local TABS = {'Deck', 'Disc', 'Relics', 'Menu'}
local OPTS = {'Resume', 'Save & Title', 'Exit', 'Abandon'}

function screen.enter(arg)
    if arg then
        tab = arg.tab or tab
        page = arg.page or 0
        retScreen = arg.return_screen or state.savedScreen or retScreen
    else
        retScreen = state.savedScreen or retScreen
    end
    menuIdx = 1
end

function screen.draw()
    local m=U.metrics();U.header('Grimoire',TABS[tab])
    screen.tabs={};screen.menu={}
    for i,label in ipairs(TABS)do
        local cw=(w-52)//4
        screen.tabs[i]=U.button(label,{x=14+(i-1)*(cw+8),y=78,w=cw,h=40},i==tab)
    end
    local y=130
    capacity=math.max(1,(m.body_bottom-y-32)//70)
    if tab==4 then
        for i,label in ipairs(OPTS)do screen.menu[i]=U.button(label,{x=30,y=y+(i-1)*54,w=w-60,h=44},i==menuIdx)end
        draw_actions('Back','Confirm','Up','Down')
    else
        local list=tab==1 and state.deck or tab==2 and state.discardPile or state.relics
        local maxP=math.max(0,(#list-1)//capacity);page=math.min(page,maxP)
        for i=page*capacity+1,math.min(#list,(page+1)*capacity)do
            local id=list[i];local row=y+(i-page*capacity-1)*70
            U.panel({x=14,y=row,w=w-28,h=64})
            local x,width=24,w-48
            local title,desc
            if tab<=2 then local c=kCards[id+1];title=c.cost..' INK '..c.name..' / '..c.meter;desc=c.desc
            else
                if not kRelics then dofile('relics.lua')end
                local r=kRelics[id+1];title=r.name;desc=r.desc
                smudge.draw_sprite(20,row+8,48,48,'sprites/relic_'..id..'.raw');x=80;width=w-104
            end
            U.text(x,row+4,U.fit(title,width,'small',true),'small',true)
            local dy=row+26
            for _,line in ipairs(smudge.wrapped_text(desc,width,'small',false,2).lines or {})do U.text(x,dy,line,'small');dy=dy+smudge.line_height('small')end
        end
        U.text(w//2,m.body_bottom-25,(page+1)..'/'..(maxP+1),'small',false,'center')
        draw_actions('Back','Tab','<','>')
    end
end

function screen.on_button(btn, pressed)
    if not pressed then return end
    if btn == 'back' then set_screen(retScreen) return end

    if btn == 'up' or btn == 'page_back' then
        tab = (tab - 2 + 4) % 4 + 1
        page, menuIdx = 0, 1
    elseif btn == 'down' or btn == 'page_forward' or (tab <= 3 and btn == 'confirm') then
        tab = (tab % 4) + 1
        page, menuIdx = 0, 1
    elseif tab == 4 then
        if btn == 'confirm' then
            if menuIdx == 1 then return set_screen(retScreen) end
            if menuIdx < 4 then save_game_state() else delete_save_state() state.inRun = false end
            if menuIdx == 3 then smudge.exit() else set_screen('title') end
            return
        end
        menuIdx = (btn == 'left') and ((menuIdx - 2 + 4) % 4 + 1) or ((menuIdx % 4) + 1)
    else
        local list = (tab == 1) and state.deck or ((tab == 2) and state.discardPile or state.relics)
        local maxP = math.floor((#list - 1) / capacity)
        if btn == 'left' and page > 0 then page = page - 1
        elseif btn == 'right' and page < maxP then page = page + 1 end
    end
    smudge.request_update()
end

function screen.on_tap(x,y)
    for i,r in ipairs(screen.tabs or {})do if U.hit(r,x,y)then tab=i;page=0;smudge.request_update();return end end
    for i,r in ipairs(screen.menu or {})do if U.hit(r,x,y)then menuIdx=i;screen.on_button('confirm',true);return end end
end
return screen
