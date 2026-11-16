local load=dofile;local rendered={}
dofile=function(path)local m=load(path);if path=='graphics.lua'then local text=m.text;m.text=function(value,...)rendered[value]=true;return text(value,...)end end;return m end
local app=dofile('main.lua');on_init();on_draw();local V,g=app.view,app.state();local export=dofile('export_wave.lua')
local function tap(action)on_draw();for _,h in ipairs(V.hit)do if h.action==action then on_touch(h.box.x+h.box.w//2,h.box.y+h.box.h//2,'tap');on_draw();return end end;error('missing '..tostring(action))end
tap('mode');tap('start');tap('ready')
for round=0,1 do
 tap(1);on_long_press(240,680);on_draw();on_touch_release(240,680);on_draw();tap('clue');tap('dial');tap(3);rendered={};on_draw()
 assert(g.phase==4 and g.mode==2 and g.round==round)
 if round==0 then assert(rendered['PRACTICE ROUND. NOTHING SCORES.']);capture('practice-caption.pgm');export(app,'practice-caption')
 else assert(rendered['SIDE CALL. THIS ROUND SCORES.']and not rendered['PRACTICE ROUND. NOTHING SCORES.'],'B12-03: scored Teams call falsely says practice');capture('scored-caption.pgm');export(app,'scored-caption')end
 tap(1);if round==0 then tap('next');tap('ready')end
end
-- Exact old/new caption ink masks, using shipped font bytes at the unchanged slot.
local F=dofile('graphics.lua');F.init();local w,h=smudge.get_bounds()
for _,row in ipairs({{'old','PRACTICE ROUND. NOTHING SCORES.'},{'new','SIDE CALL. THIS ROUND SCORES.'}})do
 smudge.clear();F.inkText(row[2],F.box(16,h-96,w-32,29),14,false,false,false);capture('caption-'..row[1]..'.pgm')
end
on_exit()
