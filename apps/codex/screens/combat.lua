local screen = {}
local U=dofile('unified_ui.lua')
local page=0
function screen.enter(arg)
    state.phase='battle'
    screen.cards=nil;screen.endTurn=nil
    if not state.enemy or (arg and arg.newCombat) then init_combat() end
end
local function enemy_panel(r)
    U.panel(r)
    local e=state.enemy
    local x,y=r.x+10,r.y+8
    local tw=r.w-158
    U.text(x,y,U.fit(string.upper(e.name),tw,'ui10',true),'ui10',true)
    U.panel({x=r.x+r.w-142,y=r.y+8,w=134,h=134})
    smudge.draw_sprite(r.x+r.w-139,r.y+11,128,128,'sprites/monster_'..tostring(e.spriteId or 0)..'.raw')
    local by=y+30
    smudge.rect(x,by,tw,12,false)
    if e.maxHp>0 then smudge.rect(x+2,by+2,math.max(0,(tw-4)*e.hp//e.maxHp),8,true) end
    U.text(x,by+18,e.hp..'/'..e.maxHp..' HP (WARD '..e.ward..')','small')
    if tw<220 then
        U.text(x,by+40,'INTENT:','small',true)
        U.text(x,by+60,string.upper(e.intent)..' ('..e.intentVal..')','small',true)
        U.text(x,by+84,U.fit('RHYTHM: '..(state.lastPlayedMeter or 'None'),tw,'small'),'small')
    else
        U.text(x,by+43,U.fit('INTENT: '..string.upper(e.intent)..' ('..e.intentVal..')',tw,'ui10',true),'ui10',true)
        U.text(x,by+72,U.fit('RHYTHM: '..(state.lastPlayedMeter or 'None'),tw,'small'),'small')
    end
end
function screen.draw()
    local m=U.metrics();w,h=m.w,m.h
    U.header('Codex Delve','CH.1 PG '..state.floor..'/15')
    U.text(14,80,'HP '..state.playerHp..'/'..state.playerMaxHp,'ui10',true)
    U.text(w//2,80,'WARD '..state.playerWard,'ui10',true,'center')
    U.text(w-14,80,'INK '..state.ink..'/'..state.maxInk,'ui10',true,'right')
    local n=#state.hand
    if n>0 then page=(math.max(1,math.min(n,state.selectedHandIdx))-1)//4 end
    page=math.min(page,math.max(0,(n-1)//4))
    local handX,handY,handW,handH
    if m.landscape then
        enemy_panel({x=14,y=114,w=336,h=156})
        if n>0 then render_card_inspector(14,278,336,122,kCards[state.hand[state.selectedHandIdx]+1],true) end
        handX,handY,handW,handH=362,114,w-376,244
    else
        enemy_panel({x=14,y=114,w=w-28,h=156})
        if n>0 then render_card_inspector(14,282,w-28,104,kCards[state.hand[state.selectedHandIdx]+1],true) end
        handX,handY,handW,handH=14,398,w-28,240
    end
    screen.cards={}
    local count=math.min(4,n-page*4)
    local cw=(handW-8*math.max(0,count-1))//math.max(1,count)
    for j=1,count do
        local i=page*4+j
        local r={x=handX+(j-1)*(cw+8),y=handY,w=cw,h=handH,id=state.hand[i],index=i}
        screen.cards[#screen.cards+1]=r
        draw_card_item(r.x,r.y,r.w,r.h,kCards[r.id+1],i==state.selectedHandIdx)
    end
    if n==0 then U.text(handX,handY+30,'Hand empty - End Turn','ui10',true)end
    local y=m.landscape and 370 or 654
    local ew=(handW-24)//4
    screen.extra={}
    for i,label in ipairs({'Deck','Discard','<','>'}) do
        screen.extra[i]=U.button(label,{x=handX+(i-1)*(ew+8),y=y,w=ew,h=40})
    end
    if not m.landscape then
        local notice=(state.pendingDiscard or 0)>0 and 'Select a card to discard' or n>4 and ('HAND '..(page+1)..'/'..((n+3)//4))or ''
        U.text(14,m.footer_y-34,notice,'small',true)
    elseif (state.pendingDiscard or 0)>0 then
        U.text(14,400,'Discard '..state.pendingDiscard..' card(s)','small',true)
    end
    screen.endTurn=U.footer({'End Turn'},1)[1]
end
function screen.on_button(btn,pressed)
    if not pressed then return end
    screen.cards=nil;screen.endTurn=nil;screen.extra=nil
    if btn=='up' or btn=='page_back' then set_screen('deck_view',{tab=1});return
    elseif btn=='down' or btn=='page_forward' then set_screen('deck_view',{tab=2});return end
    local n=#state.hand
    if btn=='back' then end_player_turn()
    elseif btn=='confirm' then play_card(state.selectedHandIdx)
    elseif btn=='left' and n>0 then state.selectedHandIdx=(state.selectedHandIdx-2+n)%n+1
    elseif btn=='right' and n>0 then state.selectedHandIdx=state.selectedHandIdx%n+1 end
    smudge.request_update()
end
function screen.on_tap(x,y)
    if U.hit(screen.endTurn,x,y) then screen.on_button('back',true);return end
    for i,r in ipairs(screen.extra or {})do
        if U.hit(r,x,y)then
            if i<=2 then screen.on_button(i==1 and 'up' or 'down',true)
            else
                local n=#state.hand
                if n>0 then state.selectedHandIdx=math.min(n,((page+(i==3 and -1 or 1)+((n+3)//4))%((n+3)//4))*4+1)end
                screen.cards=nil;screen.endTurn=nil;screen.extra=nil;smudge.request_update()
            end
            return
        end
    end
    for _,r in ipairs(screen.cards or {})do
        if U.hit(r,x,y)and state.hand[r.index]==r.id then
            local i=r.index
            screen.cards=nil;screen.endTurn=nil;screen.extra=nil
            if state.selectedHandIdx==i then play_card(i) else state.selectedHandIdx=i end
            smudge.request_update();return
        end
    end
end
return screen
