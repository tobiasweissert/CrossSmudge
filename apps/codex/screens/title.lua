-- Codex: Ink & Iron — Title Screen

local screen = {}
local title_idx = 1

function screen.enter()
    title_idx = 1
end

function screen.draw()
    local m=U.metrics()
    U.header('Codex Delve','Ink & Iron')
    local x=m.landscape and 220 or 14
    local width=w-x-14
    local y=m.landscape and 88 or 260
    draw_monster(m.landscape and 36 or (w-136)//2,100,6)
    local options=state.inRun and {string.format('Continue Delve (Page %d/15)',state.floor),'New Delve','Inspect Grimoire','Abandon Delve'} or {'New Delve','Inspect Grimoire','Exit to Applications'}
    screen.options={}
    for i,label in ipairs(options)do screen.options[i]=U.button(label,{x=x,y=y+(i-1)*52,w=width,h=44},i==title_idx)end
    if state.saveNotice then
        local ny=m.body_bottom-60
        for _,line in ipairs(smudge.wrapped_text(state.saveNotice,w-28,'small',true,3).lines or {})do U.text(14,ny,line,'small',true);ny=ny+smudge.line_height('small')end
    end
    draw_actions('Back','Select','Up','Down')
end

function screen.on_button(btn, pressed)
    if not pressed then return end
    local totalOptions = state.inRun and 4 or 3
    if btn == "back" then
        smudge.exit()
    elseif btn == "up" or btn == "left" or btn == "page_back" then
        title_idx = (title_idx - 2 + totalOptions) % totalOptions + 1
        smudge.request_update()
    elseif btn == "down" or btn == "right" or btn == "page_forward" then
        title_idx = (title_idx % totalOptions) + 1
        smudge.request_update()
    elseif btn == "confirm" then
        if state.inRun then
            if title_idx == 1 then
                -- Continue Delve
                set_screen(state.savedScreen or "combat")
            elseif title_idx == 2 then
                -- New Delve
                start_new_run()
            elseif title_idx == 3 then
                -- Inspect Grimoire
                set_screen("deck_view", {tab = 1, page = 0, return_screen = "title"})
            elseif title_idx == 4 then
                -- Abandon Delve
                delete_save_state()
                state.inRun = false
                title_idx = 1
                smudge.request_update()
            end
        else
            if title_idx == 1 then
                start_new_run()
            elseif title_idx == 2 then
                set_screen("deck_view", {tab = 1, page = 0, return_screen = "title"})
            elseif title_idx == 3 then
                smudge.exit()
            end
        end
    end
end

function screen.on_tap(x,y)
    for i,r in ipairs(screen.options or {})do if U.hit(r,x,y)then title_idx=i;screen.on_button('confirm',true);return end end
end

return screen
