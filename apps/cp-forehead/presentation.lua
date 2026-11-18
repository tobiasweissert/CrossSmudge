local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_presentation.lua')end
-- Port of CrossPlay ForeheadScreens: original chrome, category art, cards and records.
local F=dofile('graphics.lua')
local V={hit={},focus=1,page=1,screen='menu',confirmReset=false}
local art={};local previous,orientation;local played=dofile('played.lua')
function V.init(g)
 F.init();played.init(g);for n=1,17 do art[n]=assert(smudge.read_file('art/category'..n..'.raw',128)) end
 local ok,value=pcall(smudge.get_orientation);V.originalOrientation=ok and value or nil
 V.page=math.max(1,(g.review-1)//12+1)
end
function V.exit()if V.originalOrientation then pcall(smudge.set_orientation,V.originalOrientation) end end
local function hit(b,action,value)return F.hit(V.hit,b,action,value)end
local function button(label,b,action,value,solid,size)
 F.button(label,b,solid,size);hit(b,action,value)
end
local U=dofile('unified_ui.lua')
local function footer(labels,actions,primary,values)
 for i,b in ipairs(U.footer(labels,primary))do hit(b,actions[i],values and values[i])end
end
local function line(label,x,y,width,font,bold,align)
 U.text(x,y,U.fit(label,width,font or 'ui10',bold),font or 'ui10',bold,align or 'left')
end
local function bodyTop()return V.notice~=''and 116 or 78 end
local function menu(g,M,w,h)
 U.header('FOREHEAD','SCORE '..g.got);local m=U.metrics();local y=bodyTop()
 local c=M.categories[g.category];local best=tonumber(g.bestIn:sub((g.category-1)*3+1,g.category*3))or 0
 line(c[1],14,y,w-28,'lexend16',true);line(c[2],14,y+48,w-28)
 line(played.has(g.category)and'BEST '..best or'NEW CATEGORY',14,y+80,w-28)
 line(g.rounds..' ROUNDS  /  '..g.total..' WORDS',14,y+116,w-28)
 local rh=V.notice~=''and 40 or 50;local top=h-68-3*(rh+8)+8
 for i,item in ipairs({{'CATEGORY',c[1],'picker'},{'SETTINGS',g.length..' SECONDS','settings'},{'HOW TO PLAY','','help'}})do
  local b={x=14,y=top+(i-1)*(rh+8),w=w-28,h=rh};U.button(item[1]..'  '..item[2],b);hit(b,item[3])
 end
 footer({'Scores','Play'},{'records',4},2)
end
local function records(g,w,h)
 U.header('FOREHEAD','SCORES');line(g.rounds..' ROUNDS / '..g.total..' WORDS / BEST '..g.best,14,bodyTop()+2,w-28,'ui10',true)
 local count=#g.recent//3;local cw=(w-52)//4;local rh=math.min(94,(h-bodyTop()-132)//4)
 for i=1,count do local b={x=14+(i-1)%4*(cw+8),y=bodyTop()+46+(i-1)//4*(rh+6),w=cw,h=rh};U.panel(b);U.text(b.x+cw//2,b.y+(rh-25)//2,tostring(tonumber(g.recent:sub((i-1)*3+1,i*3))or 0),'ui10',true,'center')end
 if count==0 then line('Your scores show up here',14,144,w-28)end
 footer({'Menu'},{'menu'})
end
local function picker(g,M,w,h)
 U.header('CATEGORY','17 LISTS');local per=math.max(1,(h-bodyTop()-86)//58);local pages=(17+per-1)//per;V.pages=pages;V.page=math.max(1,math.min(pages,V.page))
 for n=(V.page-1)*per+1,math.min(17,V.page*per)do
  local b={x=14,y=bodyTop()+(n-1)%per*58,w=w-28,h=50};U.panel(b,n==g.category)
  smudge.bitmap(22,b.y+9,32,32,art[n],n==g.category)
  local best=tonumber(g.bestIn:sub((n-1)*3+1,n*3))or 0
  U.text(64,b.y+12,U.fit(M.categories[n][1]..' / '..(played.has(n)and'BEST '..best or'NEW'),w-92,'ui10'), 'ui10',false,'left',n~=g.category);hit(b,'category',n)
 end
 footer({'<','Menu','>'},{'page','menu','page'},nil,{math.max(1,V.page-1),nil,math.min(pages,V.page+1)})
end
local helpPages={
 {'Hold the device against your forehead, screen facing out. Everybody else gives clues. One point for every word you get right.'},
 {'Pass skips a word without a penalty. Got it scores one point. The remaining time appears below the title. Hardware upper/confirm keys mean Got it; lower keys mean Pass.'},
 {'Results show words you got, passed, or held when time ran out. Play again keeps your category. Done saves your completed round and returns to the menu.'}}
local function help(w,h)
 U.header('HOW TO PLAY',V.page..'/3');V.pages=3
 F.block(helpPages[V.page][1],F.box(14,bodyTop()+12,w-28,h-bodyTop()-100),20,14)
 footer({'<','Menu','>'},{'page','menu','page'},nil,{math.max(1,V.page-1),nil,math.min(3,V.page+1)})
 hit(F.box(14,bodyTop()+12,w-28,h-bodyTop()-100),'help-next')
end
local function settings(g,w,h)
 U.header('SETTINGS','');local y=bodyTop()+12
 for i,item in ipairs({{'ROUND '..g.length..' SECONDS',3},{V.confirmReset and'TAP AGAIN TO WIPE'or'RESET EVERYTHING','reset'}})do
  local b={x=14,y=y+(i-1)*80,w=w-28,h=64};U.button(item[1],b);hit(b,item[2])
 end
 line(V.confirmReset and'This cannot be undone' or'Scores, words and settings',14,y+166,w-28)
 if V.confirmReset then V.resetShown=true end
 footer({'Menu'},{'menu'})
end
local function ready(g,M,w,h)
 U.header('FOREHEAD','READY');local m=U.metrics()
 F.block(M.categories[g.category][1],F.box(14,bodyTop(),w-28,(m.body_bottom-bodyTop())//2),44,3)
 line('Hold it on your forehead',w//2,h//2+20,w-28,'ui10',true,'center')
 line(g.length..' seconds / either key starts',w//2,h//2+58,w-28,'ui10',false,'center')
 footer({'Start'},{1},1)
end
local function play(g,M,w,h)
 U.header('FOREHEAD','SCORE '..g.got)
 local timer={x=w-214,y=bodyTop(),w=200,h=36};U.button(math.ceil(g.left/1000)..'s / Pause',timer);hit(timer,3)
 local m=U.metrics();F.cardText(M.word(g.word),F.box(14,bodyTop()+38,w-28,m.body_bottom-bodyTop()-54),m.body_bottom-bodyTop()-54)
 footer({'Pass','Got it'},{2,1},2)
end
local function results(g,M,w,h)
 U.header('FOREHEAD','SCORE '..g.got);local total=#g.history//5;V.pages=math.max(1,(total+11)//12);V.page=math.max(1,math.min(V.pages,V.page))
 line(g.got..' GOT / '..total..' WORDS',14,bodyTop()+2,w-28,'ui10',true)
 local rh=math.max(27,(h-bodyTop()-172)//6);local cw=(w-36)//2
 for slot=0,math.min(11,total-(V.page-1)*12-1)do
  local i=(V.page-1)*12+slot;local word=tonumber(g.history:sub(i*5+1,i*5+4),16);local mark=tonumber(g.history:sub(i*5+5,i*5+5))
  local x=14+(slot//6)*(cw+8);local y=bodyTop()+38+(slot%6)*rh
  line((mark==1 and'+ 'or mark==2 and'- 'or'= ')..M.word(word),x,y,cw)
 end
 local y=h-116;for i,label in ipairs({'<',V.page..'/'..V.pages,'>'})do
  local b={x=14+(i-1)*(w-28)//3,y=y,w=(w-28)//3-4,h=44};U.button(label,b);if i~=2 then hit(b,'page',math.max(1,math.min(V.pages,V.page+(i==1 and -1 or 1))))end
 end
 footer({'Play again','Done'},{'again',3},1)
end
function V.persist(g)return played.save(g)end
function V.mustPersistBeforeAction(action)return played.needsClear and(action==4 or action=='again')end
function V.draw(g,M,notice,focus)
 V.notice=notice
 played.observe(g)
 if previous~=g.phase then if g.phase==3 then smudge.full_refresh() end;V.focus=1;V.confirmReset=false;if g.phase==0 then V.screen='menu' end;if g.phase==3 then V.page=1 end;previous=g.phase end
 local w,h=smudge.get_bounds();V.hit={};smudge.clear();V.drawnPhase,V.drawnScreen=g.phase,V.screen
 if g.phase==0 then
  if V.screen=='records'then records(g,w,h)elseif V.screen=='picker' then picker(g,M,w,h) elseif V.screen=='settings' then settings(g,w,h) elseif V.screen=='help' then help(w,h) else menu(g,M,w,h) end
 elseif g.phase==2 then ready(g,M,w,h) elseif g.phase==1 then play(g,M,w,h) else results(g,M,w,h) end
 if V.keyboard and V.hit[V.focus] and g.phase==0 and V.screen~='picker'and V.screen~='help'then F.rect(V.hit[V.focus].box,false,false,3) end
 if notice~=''then U.text(14,78,U.fit(notice,w-28,'ui10'),'ui10',true)end
end
function V.act(g,M,action,value)
 if V.drawnPhase~=g.phase or V.drawnScreen~=V.screen then return end
 if type(action)=='number' then return M.act(g,action) end
 if action=='records'then V.screen='records'
 elseif action=='picker' then V.screen='picker';V.page=(g.category-1)//9+1
 elseif action=='settings' then V.screen='settings'
 elseif action=='help' then V.screen='help';V.page=1
 elseif action=='help-next' then if V.page==3 then V.screen='menu' else V.page=V.page+1 end
 elseif action=='page' then V.page=value;if g.phase==3 then g.review=(value-1)*12+1 end
 elseif action=='category' then while g.category~=value do M.act(g,1) end;V.screen='menu'
 elseif action=='reset' then if V.confirmReset and V.resetShown then V.confirmReset=false;V.screen='menu';played.reset();return M.new(g.rng) else V.confirmReset=true;V.resetShown=false end
 elseif action=='menu' then V.screen='menu';V.confirmReset=false
 elseif action=='again' then M.act(g,3);M.act(g,4) end
end
function V.back(g)
 if g.phase==0 and V.screen~='menu' then V.screen='menu';V.confirmReset=false;return true end
 return false
end
function V.key(g,b)
 if g.phase==2 and b~='back' then return {action=1} end
 if g.phase==1 then if b=='up' or b=='left' or b=='page_back' or b=='confirm' then return {action=1} elseif b=='down' or b=='right' or b=='page_forward' then return {action=2} end end
 if (V.screen=='picker' or V.screen=='help' or g.phase==3) and (b=='up' or b=='left' or b=='page_back' or b=='down' or b=='right' or b=='page_forward') then
  local delta=(b=='up' or b=='left' or b=='page_back') and -1 or 1;return {action='page',value=(V.page-1+delta)%math.max(1,V.pages or 1)+1}
 end
 if b=='confirm' then return V.hit[V.focus] end
 if b=='up' or b=='left' or b=='page_back' then V.focus=(V.focus-2)%math.max(1,#V.hit)+1
 elseif b=='down' or b=='right' or b=='page_forward' then V.focus=V.focus%math.max(1,#V.hit)+1 end
end
function V.at(x,y)return F.at(V.hit,x,y) end
function V.clock(g)return g.phase==1 and math.ceil(g.left*8/(g.length*1000)) or g.phase end
return V
