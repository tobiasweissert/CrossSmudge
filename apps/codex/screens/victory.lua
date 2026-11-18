-- Codex: Ink & Iron — Victory Screen

local screen = {}

function screen.draw()
    local m=U.metrics();U.header('Victory','PG '..state.floor..'/15')
    U.text(w//2,m.body_top+80,'The Codex is written','ui12',true,'center')
    draw_actions('Back','Play Again','','')
end

function screen.on_button(btn, pressed)
    if not pressed then return end
    if btn == "back" or btn == "confirm" then
        delete_save_state()
        state.inRun = false
        set_screen("title")
    end
end

function screen.on_tap(x,y)
    -- Use the painted footer to leave this scene.
end

return screen
