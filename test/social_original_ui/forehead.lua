local read,write=smudge.read_file,smudge.write_file;local saved={};local now=123456
smudge.millis=function()return now end
smudge.read_file=function(path,n,offset)if path:match('%.dat$')then local data=saved[path];return data and data:sub((offset or 0)+1,(offset or 0)+n)end;return read(path,n,offset)end
smudge.write_file=function(path,data)saved[path]=data;return true end
local expectedPlayed=0;local before=smudge.get_orientation();local app=dofile('main.lua');on_init();on_draw()
local function hit(action,value)
 for _,target in ipairs(app.view.hit)do if target.action==action and(value==nil or target.value==value)then local b=target.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('missing actual Forehead action '..tostring(action)..'/'..tostring(value))
end
local function shot(name)
 local g=app.state();local fields={g.category,g.length,g.left,g.got,g.phase,app.view.page,app.view.confirmReset and 1 or 0,g.rounds,g.total,g.best,expectedPlayed}
 for i,v in ipairs(fields)do fields[i]=tostring(v)end
 local word=g.word>0 and app.core.word(g.word)or'-'
 assert(write(name..'.state',table.concat(fields,' ')..'\n'..app.view.screen..'\n'..word..'\n'..g.bestIn..'\n'..(g.recent==''and'-'or g.recent)..'\n'..(g.history==''and'-'or g.history)..'\n'..g.seen,false));capture(name..'.pgm')
end
shot('forehead-menu');hit('picker');shot('forehead-picker-one');on_button('page_forward',true);on_draw();shot('forehead-picker-two');hit('category',17);shot('forehead-menu-tricky')
hit('settings');shot('forehead-settings');hit(3);shot('forehead-settings-90');on_button('back',true);on_draw();hit('help')
for page=1,3 do shot('forehead-help-'..page);hit('help-next')end
assert(app.view.screen=='menu');hit(4);assert(app.state().phase==2);shot('forehead-ready');on_button('confirm',true);on_draw();assert(app.state().phase==1);shot('forehead-play')
for i=1,13 do on_button(i%3==0 and 'page_forward'or'page_back',true);on_draw();if i==1 or i==5 or i==13 then shot('forehead-play-'..i)end end
now=now+90001;on_update();on_draw();assert(app.state().phase==3 and app.state().rounds==1);expectedPlayed=1<<16;shot('forehead-result-one');on_button('page_forward',true);on_draw();shot('forehead-result-two')
hit(3);shot('forehead-menu-record');hit('settings');shot('forehead-settings-record');hit('reset');shot('forehead-reset-armed');on_button('back',true);on_draw();on_update();on_exit();assert(smudge.get_orientation()==before)
app=dofile('main.lua');on_init();on_draw();assert(app.state().rounds==1 and app.state().category==17 and app.state().length==90);on_exit();assert(smudge.get_orientation()==before)
app=dofile('main.lua');on_init();on_draw();hit('picker');on_button('page_back',true);on_draw();hit('category',1);hit(4);on_button('confirm',true);on_draw();now=now+90001;on_update();on_draw();assert(app.state().phase==3 and app.state().got==0);expectedPlayed=expectedPlayed|1;hit(3);shot('forehead-menu-zero-score');on_exit()
app=dofile('main.lua');on_init();on_draw();shot('forehead-menu-zero-reopened');on_exit()
app=dofile('main.lua');on_init();on_draw();hit(4);on_button('confirm',true);on_draw()
for score=1,100 do on_button('page_back',true);on_draw();if score==10 or score==16 or score==100 then assert(app.state().got==score);shot('forehead-live-score-'..score)end end
now=now+90001;on_update();on_draw();assert(app.state().got==100 and app.state().phase==3);shot('forehead-result-100');on_button('page_back',true);on_draw();shot('forehead-result-100-last');hit(3);shot('forehead-menu-100');on_exit()
print('Actual Forehead category/settings/help/ready/word/Got/Pass/unanswered/results/records/reopen passed')
