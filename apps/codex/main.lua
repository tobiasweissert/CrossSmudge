-- Codex: Ink & Iron for CrossSmudge (Lua)
-- Modular roguelike deckbuilder with dynamic screen loading and low heap footprint

w, h = 480, 800
U = dofile('unified_ui.lua')
local footer, painted = {}, false
function draw_actions(a,b,c,d)
    local labels={a,b,c,d};local enabled={}
    for i=1,4 do enabled[i]=labels[i]~=nil and labels[i]~='' end
    footer=U.footer(labels,2,enabled)
end

dofile("state.lua")
dofile("ui_base.lua")
collectgarbage("collect")

active_screen = nil
active_screen_name = nil
pending_screen = nil
pending_screen_arg = nil

function set_screen(name, arg)
    painted=false;footer={}
    pending_screen = name
    pending_screen_arg = arg
end

function apply_screen_transition()
    if not pending_screen then return end
    local name = pending_screen
    local arg = pending_screen_arg
    pending_screen = nil
    pending_screen_arg = nil

    if active_screen_name == name and active_screen then
        collectgarbage("collect")
        if active_screen.enter then
            active_screen.enter(arg)
        end
        smudge.request_update()
        return
    end

    if active_screen and active_screen.exit then
        active_screen.exit()
    end
    active_screen = nil
    collectgarbage("collect")
    collectgarbage("collect")

    if not kCards and (name == "combat" or name == "reward" or name == "shop" or name == "deck_view") then
        dofile("cards.lua")
        collectgarbage("collect")
    end

    if name == "combat" or name == "reward" then
        if not play_card then
            dofile("combat_engine.lua")
            collectgarbage("collect")
        end
        if not draw_card_item then
            dofile("card_ui.lua")
            collectgarbage("collect")
        end
    end

    active_screen_name = name
    active_screen = dofile("screens/" .. name .. ".lua")
    collectgarbage("collect")

    if active_screen and active_screen.enter then
        active_screen.enter(arg)
    end
    collectgarbage("collect")
    smudge.request_update()
end

function on_init()
    w, h = smudge.get_bounds()
    math.randomseed(os.time())
    local loader = dofile("load_save.lua")
    if loader then loader() end
    loader = nil
    collectgarbage("collect")
    set_screen("title")
    apply_screen_transition()
end

function on_draw()
    if pending_screen then
        apply_screen_transition()
    end
    smudge.clear()
    w,h=smudge.get_bounds();footer={}
    if active_screen and active_screen.draw then
        active_screen.draw()
    end
    painted=true
    collectgarbage("step", 50)
end

function on_button(btn, pressed)
    if pressed then painted=false;footer={} end
    if active_screen and active_screen.on_button then
        active_screen.on_button(btn, pressed)
    end
    if pending_screen then
        apply_screen_transition()
    end
    collectgarbage("step", 50)
end

function on_tap(x, y)
    if not painted then return end
    painted=false
    for i,r in ipairs(footer)do
        if U.hit(r,x,y)then on_button(({'back','confirm','left','right'})[i],true);return end
    end
    if y>=h-68 then
        if active_screen_name=='combat' and active_screen and active_screen.on_tap then active_screen.on_tap(x,y)end
        return
    end
    if y<64 then return end

    if active_screen and active_screen.on_tap then
        active_screen.on_tap(x, y)
    end
end
