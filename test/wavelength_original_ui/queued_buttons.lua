assert(smudge.get_device()=='sticky');local app=dofile('main.lua');on_init();on_draw();local V,g=app.view,app.state()
local function button(action)
 for i,h in ipairs(V.hit)do if h.action==action then V.focus=i;on_button('confirm',true);on_draw();return end end;error('missing '..tostring(action))
end
local function queued(action,phase,view)
 for i,h in ipairs(V.hit)do if h.action==action then V.focus=i;on_button('confirm',true);on_button('confirm',true);assert(g.phase==phase,'B12-02 queued physical Confirm');on_draw();assert(V.currentView==view);return end end;error('missing queued '..tostring(action))
end
button('mode');button('start');button('ready');queued(1,2,'peek');assert(not V.everRevealed and not V.peek)
local held=false;smudge.is_button_down=function()return held end
held=true;on_button('confirm',true);on_update();on_draw();assert(V.peek);held=false;on_button('confirm',false);on_draw();assert(not V.peek and V.everRevealed)
queued('clue',3,'clue');queued('dial',3,'dial');queued(3,4,'call');queued(1,5,'reveal');queued('next',5,'pass')
on_button('back',true);on_button('confirm',true);on_draw();assert(V.currentView=='menu','queued Back must not reopen pass');button('start');button('ready');queued(1,2,'peek')
on_button('back',true);on_draw();assert(V.currentView=='pause');queued('abandon',1,'pass');assert(not V.peek and not V.everRevealed);on_button('back',true);on_draw();assert(V.currentView=='menu');on_exit()
