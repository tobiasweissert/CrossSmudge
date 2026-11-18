local screen = {}
local idx = 1

local function gen_shop()
    shop_state = { cards = {}, relics = {}, heal = false }
    while #shop_state.cards < 3 do
        shop_state.cards[#shop_state.cards + 1] = math.random(0, #kCards - 1)
    end
    for i = 0, 11 do
        if not has_relic(i) then shop_state.relics[#shop_state.relics + 1] = i end
        if #shop_state.relics >= 2 then break end
    end
    while #shop_state.relics < 2 do shop_state.relics[#shop_state.relics + 1] = 0 end
    if has_relic(11) then
        state.playerHp = math.min(state.playerMaxHp, state.playerHp + 8)
    end
    state.savedScreen = "shop"
    save_game_state()
end

function screen.enter(arg)
    if (arg and arg.newShop) or not shop_state.cards or #shop_state.cards == 0 then
        gen_shop()
    end
    idx = 1
    collectgarbage("collect")
end

function screen.exit()
    collectgarbage("collect")
end

function screen.draw()
    local m=U.metrics()
    U.header('Scriptorium','PG '..state.floor..'/15')
    U.text(14,80,'HP '..state.playerHp..'/'..state.playerMaxHp..'  GOLD '..state.gold,'ui10',true)
    local x=m.landscape and 180 or 14
    local y= m.landscape and 114 or 258
    draw_miniature_frame(14,114,136,136)
    smudge.draw_sprite(18,118,128,128,'sprites/merchant.raw')
    local cap=math.max(1,(m.body_bottom-y-12)//44)
    local first=((idx-1)//cap)*cap+1
    screen.items={}
    for i=first,math.min(7,first+cap-1)do
        local label
        if i<=3 then local id=shop_state.cards[i];label=(id<0 and 'SOLD' or kCards[id+1].name)..' ('..(35+i*10)..' G)'
        elseif i<=5 then if not kRelics then dofile('relics.lua')end;local id=shop_state.relics[i-3];label=(id<0 and 'SOLD' or kRelics[id+1].name)..' ('..(95+(i-3)*15)..' G)'
        elseif i==6 then label=shop_state.heal and 'Draught SOLD' or 'Draught +12 HP (35 G)'
        else label='Turn the Page (Depart)'end
        screen.items[i]=U.button(label,{x=x,y=y+(i-first)*44,w=w-x-14,h=36},i==idx)
    end
    draw_actions('Leave','Buy','Prev','Next')
end

function screen.on_button(btn, pressed)
    if not pressed then return end
    if btn == "back" then return advance_floor() end
    if btn == "confirm" then
        if idx == 7 then return advance_floor() end
        local p = (idx <= 3) and (35 + idx * 10) or ((idx <= 5) and (95 + (idx - 3) * 15) or 35)
        if state.gold >= p then
            if idx <= 3 then
                local cId = shop_state.cards[idx]
                if cId >= 0 then
                    state.gold = state.gold - p
                    state.deck[#state.deck + 1] = cId
                    shop_state.cards[idx] = -1
                    save_game_state()
                end
            elseif idx <= 5 then
                local rIdx = idx - 3
                local rId = shop_state.relics[rIdx]
                if rId >= 0 then
                    state.gold = state.gold - p
                    if not has_relic(rId) then state.relics[#state.relics + 1] = rId end
                    shop_state.relics[rIdx] = -1
                    save_game_state()
                end
            elseif not shop_state.heal then
                state.gold = state.gold - p
                state.playerHp = math.min(state.playerMaxHp, state.playerHp + 12)
                shop_state.heal = true
                save_game_state()
            end
        end
    elseif btn == "left" or btn == "up" then
        idx = (idx - 2 + 7) % 7 + 1
    elseif btn == "right" or btn == "down" then
        idx = (idx % 7) + 1
    elseif btn == "page_back" then
        set_screen("deck_view", {tab = 1})
    elseif btn == "page_forward" then
        set_screen("deck_view", {tab = 4})
    end
    smudge.request_update()
end

function screen.on_tap(x,y)
    for i,r in pairs(screen.items or {})do if U.hit(r,x,y)then idx=i;screen.on_button('confirm',true);return end end
end

return screen
