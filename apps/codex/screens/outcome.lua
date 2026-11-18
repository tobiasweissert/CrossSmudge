local screen = {}
local text = ""

function screen.enter(arg)
    if arg and arg.choice then
        local c = arg.choice
        state.playerMaxHp = math.max(10, state.playerMaxHp + c[4])
        state.playerHp = math.max(0, math.min(state.playerMaxHp, state.playerHp + c[3]))
        if c[5] ~= 0 then state.gold = math.max(0, state.gold + c[5]) end
        if c[6] ~= 0 then state.permStrength = state.permStrength + c[6] end
        if c[7] ~= 0 then state.baseMaxInk = math.min(6, state.baseMaxInk + c[7]) end
        if c[8] ~= 0 then state.bonusWard = state.bonusWard + c[8] end
        if c[9] ~= 0 then table.insert(state.deck, c[9]) end
        if c[10] ~= 0 then
            for r = 0, 11 do
                if not has_relic(r) then table.insert(state.relics, r) break end
            end
        end
        if c[11] ~= 0 then state.playerHp = state.playerMaxHp end
        state.outcomeText = c[12] or ""
    end
    text = state.outcomeText or ""
    state.savedScreen = "outcome"
    save_game_state()
end

function screen.draw()
    local m=U.metrics();U.header('Scriptorium','Consequence')
    U.text(14,80,'HP '..state.playerHp..'/'..state.playerMaxHp..'  GOLD '..state.gold,'ui10',true)
    U.panel({x=14,y=116,w=w-28,h=m.body_bottom-128})
    local y=130
    for _,line in ipairs(smudge.wrapped_text(text,w-56,'small',false,16).lines or {})do U.text(28,y,line,'small');y=y+smudge.line_height('small')end
    draw_actions('Continue','Continue','','')
end

function screen.on_button(btn, pressed)
    if not pressed or (btn ~= 'back' and btn ~= 'confirm') then return end
    if state.playerHp <= 0 then
        state.inRun = false
        delete_save_state()
        set_screen("game_over")
    else
        advance_floor()
    end
end

function screen.on_tap(x,y)
    -- Continue is explicit in the footer; body taps do not advance a scene.
end

return screen
