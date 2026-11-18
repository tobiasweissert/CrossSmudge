local screen = {}

function screen.enter()
    state.rewardIdx = 1
end

function screen.draw()
    local m=U.metrics()
    U.header('Codex Delve','Victory')
    U.text(14,80,'Victory +'..(18+(has_relic(6)and 15 or 0))..' Gold','ui10',true)
    local n=#(state.cardRewards or {});local sel=state.rewardIdx or 1
    local x,y,cw,ch=14,248,(w-44)//3,220
    if m.landscape then x,y,cw,ch=326,114,(w-356)//3,242 end
    if n>0 then render_card_inspector(14,114,m.landscape and 300 or w-28,m.landscape and 220 or 122,kCards[state.cardRewards[sel]+1],false)end
    screen.cards={}
    for i=1,n do local r={x=x+(i-1)*(cw+8),y=y,w=cw,h=ch};screen.cards[i]=r;draw_card_item(r.x,r.y,r.w,r.h,kCards[state.cardRewards[i]+1],sel==i)end
    draw_actions('Skip','Take','<','>')
end

function screen.on_button(btn, pressed)
    if not pressed then return end
    if btn == "up" or btn == "page_back" then
        set_screen("deck_view", {tab = 1, page = 0, return_screen = "reward"})
        return
    elseif btn == "down" or btn == "page_forward" then
        set_screen("deck_view", {tab = 4, page = 0, return_screen = "reward"})
        return
    end

    local n = #(state.cardRewards or {})
    if btn == "back" then
        advance_floor()
    elseif btn == "confirm" then
        local sel = state.rewardIdx or 1
        if sel <= n and n > 0 then
            table.insert(state.deck, state.cardRewards[sel])
        end
        advance_floor()
    elseif btn == "left" and n > 0 then
        state.rewardIdx = ((state.rewardIdx or 1) - 2 + n) % n + 1
        smudge.request_update()
    elseif btn == "right" and n > 0 then
        state.rewardIdx = ((state.rewardIdx or 1) % n) + 1
        smudge.request_update()
    end
end

function screen.on_tap(x, y)
    for i,r in ipairs(screen.cards or {}) do
        if x>=r.x and x<r.x+r.w and y>=r.y and y<r.y+r.h then
            screen.cards=nil
            if (state.rewardIdx or 1)==i then
                table.insert(state.deck,state.cardRewards[i])
                advance_floor()
            else
                state.rewardIdx=i
                smudge.request_update()
            end
            return
        end
    end
end

return screen
