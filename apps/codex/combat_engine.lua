function draw_cards(n)
    for _ = 1, n do
        if #state.drawPile == 0 then
            if #state.discardPile == 0 then break end
            for _, c in ipairs(state.discardPile) do table.insert(state.drawPile, c) end
            state.discardPile = {}
            for j = #state.drawPile, 2, -1 do
                local r = math.random(j)
                state.drawPile[j], state.drawPile[r] = state.drawPile[r], state.drawPile[j]
            end
        end
        if #state.drawPile > 0 then
            table.insert(state.hand, table.remove(state.drawPile))
        end
    end
end

function spawn_enemy(n)
    local b = (n == 15)
    local u = (n == 8 or n == 11 or n == 14)
    local r = b and ((n % 2 == 0) and 11 or 6) or
                (u and ({4, 5, 9, 10})[(n % 4) + 1] or
                ({0, 1, 2, 3, 7, 8})[((n - 1) % 6) + 1])
    local e = nil
    if smudge and smudge.find_section then
        smudge.find_section("monsters.txt", "[" .. r .. "]", "[", function(line)
            local name, hp, s, intent, iv = line:match("^([^|]+)|(%d+)|(%d+)|([^|]+)|(%d+)")
            if name then
                local bVal = (not b and not u) and math.floor((n - 1) * 2.5) or 0
                hp = tonumber(hp) + bVal
                e = {
                    name = name, hp = hp, maxHp = hp, ward = 0,
                    spriteId = tonumber(s), intent = intent,
                    intentVal = tonumber(iv) + ((not b and not u) and math.floor(n / 3) or 0),
                    isElite = u, isBoss = b,
                    status = {strength = 0, weak = 0, vuln = 0}
                }
            end
            return false
        end)
    end
    return e
end

function init_combat()
    state.enemy = spawn_enemy(state.floor)

    state.pendingDiscard = 0
    state.turn = 1
    state.maxInk = state.baseMaxInk
    state.ink = state.maxInk + (has_relic(0) and 1 or 0)
    state.playerWard = state.bonusWard + (has_relic(1) and 6 or 0)
    state.bonusWard = 0
    state.lastPlayedMeter = "None"
    state.playerStatus = {strength = state.permStrength or 0, weak = 0, vuln = 0, retainWard = false, extraDraw = 0, transcendence = false}

    state.drawPile = {}
    for _, c in ipairs(state.deck) do table.insert(state.drawPile, c) end
    for j = #state.drawPile, 2, -1 do
        local r = math.random(j)
        state.drawPile[j], state.drawPile[r] = state.drawPile[r], state.drawPile[j]
    end

    state.hand = {}
    state.discardPile = {}
    draw_cards(has_relic(3) and 5 or 4)

    state.selectedHandIdx = 1
    state.savedScreen = "combat"
    save_game_state()
end

function resolve_combat()
    if state.phase ~= "battle" or not state.inRun then return true end
    if state.playerHp <= 0 then
        state.inRun = false
        delete_save_state()
        set_screen("game_over")
        return true
    end
    if state.enemy.hp <= 0 then
        state.gold = state.gold + 18 + (has_relic(6) and 15 or 0)
        if has_relic(2) then state.playerHp = math.min(state.playerMaxHp, state.playerHp + 2) end
        if state.floor == 15 then
            state.inRun = false
            delete_save_state()
            set_screen("victory")
            return true
        end
        state.cardRewards = {}
        while #state.cardRewards < 3 do table.insert(state.cardRewards, math.random(0, #kCards - 1)) end
        state.rewardIdx = 1
        state.phase = "reward"
        state.savedScreen = "reward"
        save_game_state()
        set_screen("reward")
        return true
    end
    return false
end

local function hit_enemy(amount)
    local e=state.enemy
    local blocked=math.min(e.ward,amount)
    e.ward=e.ward-blocked
    e.hp=math.max(0,e.hp-(amount-blocked))
end

function play_card(hIdx)
    if not state.inRun or state.phase ~= 'battle' then return end
    local cid=state.hand[hIdx]
    local c=cid and kCards[cid+1]
    if not c then return end
    if (state.pendingDiscard or 0)>0 then
        state.discardPile[#state.discardPile+1]=table.remove(state.hand,hIdx)
        state.pendingDiscard=state.pendingDiscard-1
        state.selectedHandIdx=math.max(1,math.min(state.selectedHandIdx,#state.hand))
        save_game_state()
        return
    end
    if state.ink<c.cost then return end
    local ps,es=state.playerStatus,state.enemy.status
    local combo=coup_on(c)
    state.ink=state.ink-c.cost
    table.remove(state.hand,hIdx)
    local function attack(d)
        d=d+(ps.strength or 0)+(has_relic(4) and c.meter=='Blade' and 1 or 0)
        if ps.weak>0 then d=math.max(1,math.floor(d*.75))end
        if es.vuln>0 then d=math.floor(d*1.5)end
        hit_enemy(d)
    end
    local b=combo and c.cBonus or 0
    if c.type=='Attack' then
        local d=c.baseVal
        if cid==0 or cid==3 or cid==4 or cid==13 or cid==24 then d=d+b end
        if cid==23 and es.vuln>0 then d=d+10 end
        local strikes=cid==10 and (combo and 3 or 2) or 1
        for _=1,strikes do attack(d)end
        if cid==6 and combo then state.ink=state.ink+1
        elseif cid==9 then es.poison=(es.poison or 0)+4+(combo and 3 or 0)
        elseif cid==17 then es.vuln=es.vuln+2
        elseif cid==19 then state.playerWard=state.playerWard+4
        elseif cid==24 then state.playerWard=state.playerWard+5
        elseif cid==27 and es.weak>0 then state.ink=state.ink+1 end
    elseif c.type=='Skill' then
        if c.meter=='Ward' then
            state.playerWard=state.playerWard+c.baseVal+((cid==1 or cid==5 or cid==15) and b or 0)
            if cid==7 and combo then es.weak=es.weak+2
            elseif cid==25 then ps.retainWard=true
            elseif cid==26 and combo then hit_enemy(math.min(12,state.playerWard))end
        elseif cid==2 then state.playerWard=state.playerWard+3;draw_cards(2);if combo then state.ink=state.ink+1 end
        elseif cid==8 then state.playerHp=math.max(0,state.playerHp-3);state.ink=state.ink+2
        elseif cid==12 then es.weak=es.weak+2;es.vuln=es.vuln+2
        elseif cid==14 then draw_cards(3)
        elseif cid==29 then
            draw_cards(2)
            state.pendingDiscard=#state.hand>0 and 1 or 0
        end
    elseif c.type=='Power' then
        if cid==20 then ps.strength=ps.strength+2
        elseif cid==21 then ps.retainWard=true
        elseif cid==22 then ps.extraDraw=ps.extraDraw+1
        elseif cid==28 then ps.transcendence=true end
    end
    if c.type~='Power' then state.discardPile[#state.discardPile+1]=cid end
    if c.meter=="Script" and has_relic(7) then
        ps.scriptPlayed=(ps.scriptPlayed or 0)+1
        if ps.scriptPlayed==3 then ps.scriptPlayed=0;state.ink=state.ink+1 end
    end
    state.lastPlayedMeter=c.meter
    state.selectedHandIdx=math.max(1,math.min(state.selectedHandIdx,#state.hand))
    if not resolve_combat() then save_game_state()end
end

function end_player_turn()
    if not state.inRun or state.phase ~= "battle" or (state.pendingDiscard or 0)>0 then return end
    for _, c in ipairs(state.hand) do table.insert(state.discardPile, c) end
    state.hand = {}

    local es=state.enemy.status
    if (es.poison or 0)>0 then
        state.enemy.hp=math.max(0,state.enemy.hp-es.poison)
        es.poison=es.poison-1
        if resolve_combat() then return end
    end
    local d = state.enemy.intentVal + ((state.enemy.status and state.enemy.status.strength) or 0)
    if state.enemy.status and state.enemy.status.weak > 0 then d = math.max(1, math.floor(d * 0.75)) end
    if state.playerStatus.vuln > 0 then d = math.floor(d * 1.5) end

    if state.enemy.intent == "Attack" or state.enemy.intent == "AttackAndDefend" then
        if state.playerWard>0 and d>0 and has_relic(8) then state.enemy.hp=math.max(0,state.enemy.hp-3)end
        if state.playerWard >= d then
            state.playerWard = state.playerWard - d

        else
            local u = d - state.playerWard
            state.playerWard = 0
            state.playerHp = math.max(0, state.playerHp - u)
        end
    end

    if state.enemy.intent == "Defend" or state.enemy.intent == "AttackAndDefend" then
        state.enemy.ward = state.enemy.ward + 8
    end

    if state.playerHp <= 0 then
        state.inRun = false
        delete_save_state()
        set_screen("game_over")
        return
    end

    if resolve_combat() then return end
    state.turn=(state.turn or 1)+1
    for _,status in ipairs({state.playerStatus,es})do
        for _,key in ipairs({"weak","vuln"})do status[key]=math.max(0,(status[key] or 0)-1)end
    end
    if state.turn==2 and has_relic(10) then state.playerStatus.weak=0;state.playerStatus.vuln=0 end
    state.ink = state.maxInk
    state.lastPlayedMeter = "None"
    if not state.playerStatus.retainWard and not has_relic(5) then
        state.playerWard = math.floor(state.playerWard / 2)
    end
    draw_cards(4 + state.playerStatus.extraDraw + ((has_relic(9) and state.turn%3==0) and 2 or 0))
    state.selectedHandIdx = 1
    save_game_state()
end
