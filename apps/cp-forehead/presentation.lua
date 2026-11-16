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
local function row(label,value,b,action,n)
 F.rect(b,false,false,1);F.nativeText(label,F.box(b.x+12,b.y+2,b.w-24,b.h-4),20,false,true)
 if value then F.nativeText(value,F.box(b.x+b.w-12-F.inkWidth(value,20),b.y+2,F.inkWidth(value,20),b.h-4),20,false,true);end
 hit(b,action,n)
end
local function menu(g,M,w,h)
 local c=M.categories[g.category];F.header('FOREHEAD',w)
 local play=F.box(16,119,w-32,140);F.rect(play,false,false,3)
 local titleSize=F.fit(c[1],w-120,30);F.text(c[1],32,137+(52-F.cap[titleSize])//2,titleSize)
 F.text(c[2],32,201,F.fit(c[2],w-120,14))
 local best=tonumber(g.bestIn:sub((g.category-1)*3+1,g.category*3)) or 0
 F.text(played.has(g.category)and 'BEST '..best or'NEW',32,228,14)
 -- Original fillPolygon scans strictly below each edge's upper endpoint.
 for y=168,211 do local reach=44-2*math.abs(y-189);smudge.rect(w-76,y,reach+1,1,true,true)end
 hit(play,4);smudge.rect(16,271,w-32,3,true,true)
 F.text(g.rounds>0 and ('TOTAL  '..g.rounds..(g.rounds==1 and ' ROUND  ' or ' ROUNDS  ')..g.total..(g.total==1 and ' WORD' or ' WORDS')) or 'NO ROUNDS PLAYED YET',16,291,14)
 local doors=h-16-194;local panel=F.box(16,326,w-32,math.max(70,doors-338));F.corners(panel,22,3)
 local count=#g.recent//3
 if count==0 then F.nativeText('YOUR SCORES SHOW UP HERE',panel,14,false,false,true)
 else
  local peak=1;for i=1,count do peak=math.max(peak,tonumber(g.recent:sub((i-1)*3+1,i*3)) or 0) end
  local step=(panel.w-52)//16;local left=panel.x+(panel.w-step*16)//2;local base=panel.y+panel.h-32
  smudge.rect(left,base,step*16,1,true,true)
  for i=1,count do local value=tonumber(g.recent:sub((i-1)*3+1,i*3)) or 0;local bh=6+value*(panel.h-64)//peak
   local b=F.box(left+(16-count+i-1)*step+3,base-bh,step-6,bh)
   F.rect(b,true,i~=count);F.rect(b,false,false,2)
  end
 end
 row('CATEGORY',c[1],F.box(16,doors,w-32,62),'picker')
 row('SETTINGS',g.length..' SECONDS',F.box(16,doors+66,w-32,62),'settings')
 row('HOW TO PLAY',nil,F.box(16,doors+132,w-32,62),'help',1)
end
local function picker(g,M,w,h)
 F.header('CATEGORY',w,'17 LISTS');local per=math.min(9,math.max(1,(h-180)//66));local pages=(17+per-1)//per;V.pages=pages
 V.page=math.max(1,math.min(pages,V.page));local first=(V.page-1)*per+1
 for n=first,math.min(17,first+per-1) do local b=F.box(16,119+(n-first)*66,w-32,62)
  F.rect(b,n==g.category,false,1);smudge.bitmap(28,b.y+15,32,32,art[n],n==g.category)
  local title=M.categories[n][1];local size=F.fit(title,w-196,20)
  F.nativeText(title,F.box(70,b.y+2,w-180,58),20,n==g.category,true)
  local best=tonumber(g.bestIn:sub((n-1)*3+1,n*3)) or 0;local label=played.has(n)and 'BEST '..best or'NEW'
  F.nativeText(label,F.box(w-28-F.inkWidth(label,20),b.y+2,F.inkWidth(label,20),58),20,n==g.category,true);hit(b,'category',n)
 end
 F.pips(V.hit,w,h-60,pages,V.page,'page')
end
local helps={
 {'THE GAME',{'ONE PLAYER HOLDS THE DEVICE FLAT','AGAINST THEIR FOREHEAD, SCREEN','FACING OUT. THEY CANNOT SEE IT.','','EVERYBODY ELSE CAN, AND SHOUTS','CLUES UNTIL THEY GET IT. ONE','POINT FOR EVERY WORD.'}},
 {'THE TWO KEYS',{'TURN THE DEVICE SIDEWAYS AND THE','TWO PAGE KEYS LAND ON THE TOP','AND BOTTOM EDGES, WHERE YOUR','FINGERS ALREADY ARE.','','THE SCREEN LABELS BOTH EDGES SO','THE ROOM CAN CALL THEM OUT.'}},
 {'THE SCOREBOARD',{'GIVING UP COSTS NOTHING BUT THE','SECONDS IT TOOK. THE WORD IN','YOUR HAND WHEN THE CLOCK RUNS','OUT IS NEITHER, AND THE RESULTS','SAY SO RATHER THAN QUIETLY','COUNTING IT AGAINST YOU. THE','MARKS THERE MEAN:'}}}
local function help(w,h)
 V.pages=3;V.page=math.max(1,math.min(3,V.page));local page=helps[V.page];F.header('HOW TO PLAY',w,V.page..'/3')
 F.nativeText(page[1],F.box(16,119,w-32,56),30,false,true,true)
 for n,line in ipairs(page[2])do if line~=''then F.nativeText(line,F.box(16,187+(n-1)*38,w-32,38),14,false,true,true)end end
 local top=465;local room=h-60-top
 if V.page==3 then
  local rh=room//3;for n,label in ipairs({'YOU GOT IT','YOU PASSED','TIME RAN OUT'})do local cy=top+(n-1)*rh+rh//2
   if n<3 then F.disc(36,cy,9,true);if n==2 then F.disc(36,cy,7,false)end else smudge.rect(27,cy-2,18,4,true,true)end
   F.nativeText(label,F.box(62,top+(n-1)*rh,w-78,rh),14,false,true,true)
  end
 else
  local dw=math.min(w-32,room*5//3);local dh=dw*3//5;local x=(w-dw)//2
  F.rect(F.box(x,top,dw,dh),false,false,4);local band=(dh-8)*54//480
  local upper,lower=F.box(x+4,top+4,dw-8,band),F.box(x+4,top+dh-4-band,dw-8,band)
  F.rect(upper,true,false);F.rect(lower,true,false)
  if V.page==2 then F.nativeText('PASS',upper,14,true,false,true);F.nativeText('GOT IT',lower,14,true,false,true)end
  F.nativeText('PENGUIN',F.box(x,top+dh//2-22,dw,44),20,false,false,true)
 end
 F.pips(V.hit,w,h-60,3,V.page,'page');hit(F.box(16,119,w-32,h-179),'help-next')
end

local function settings(g,w,h)
 local anything=g.rounds>0 or g.category~=1 or g.length~=60 or g.seen:find('[1-9a-f]')~=nil
 F.header('SETTINGS',w)
 local b=F.box(16,119,w-32,87);F.rect(b,false,false,1)
 F.nativeText('ROUND',F.box(28,127,w-56,42),20,false,true)
 local value=g.length..' SECONDS';F.nativeText(value,F.box(w-28-F.inkWidth(value,20),127,F.inkWidth(value,20),42),20,false,true)
 F.nativeText('TAP TO CYCLE 30 60 90 120',F.box(28,169,w-56,29),14,false,true);hit(b,3)
 b=F.box(16,210,w-32,87);F.rect(b,false,false,1)
 F.nativeText(V.confirmReset and 'TAP AGAIN TO WIPE'or'RESET EVERYTHING',F.box(28,218,w-56,42),20,false,true)
 F.nativeText(not anything and 'NOTHING TO CLEAR YET'or V.confirmReset and 'THIS CANNOT BE UNDONE'or'SCORES WORDS AND SETTINGS',F.box(28,260,w-56,29),14,false,true)
 if anything then hit(b,'reset')end
 if V.confirmReset then V.resetShown=true end
end

local function bands(w,h)
 smudge.rect(0,0,w,54,true,true);smudge.rect(0,h-54,w,54,true,true)
 F.nativeText('PASS',F.box(0,0,w,54),30,true,false,true);F.nativeText('GOT IT',F.box(0,h-54,w,54),30,true,false,true)
end
local function ready(g,M,w,h)
 bands(w,h);local c=M.categories[g.category]
 F.nativeText(c[1],F.box(0,94,w,62),44,false,false,true);F.nativeText(c[2],F.box(0,168,w,42),30,false,false,true)
 F.nativeText('HOLD IT ON YOUR FOREHEAD',F.box(0,h-162,w,44),30,false,false,true)
 F.nativeText('PRESS EITHER KEY TO START',F.box(0,h-112,w,44),30,false,false,true)
 hit(F.box(math.min(140,w//4),54,w-2*math.min(140,w//4),h-108),1)
end
local function play(g,M,w,h)
 bands(w,h);local barw=w-32;local segments=math.ceil(g.left*8/(g.length*1000))
 for n=0,7 do local left=16+barw*n//8;local right=16+barw*(n+1)//8
  F.rect(F.box(left,70,right-left-4,14),n<segments,false,2)
 end
 F.text(tostring(g.got),w-16-F.inkWidth(tostring(g.got),30),96,30)
 local text=M.word(g.word);F.cardText(text,F.box(16,54,w-32,h-108),h-244)
 local inset=math.min(140,w//4);hit(F.box(inset,90,w-2*inset,h//2-90),2);hit(F.box(inset,h//2,w-2*inset,h//2-90),1)
end
local function results(g,M,w,h)
 F.header(M.categories[g.category][1],w);local total=#g.history//5;local pages=math.max(1,(total+11)//12)
 V.pages=pages;V.page=math.max(1,math.min(pages,V.page));F.inkText(tostring(g.got),F.box(16,119,140,140),64)
 F.inkText(g.got==1 and 'WORD'or'WORDS',F.box(16,265,140,32),14);F.inkText('OUT OF '..total,F.box(16,299,140,30),14)
 local band=F.box(168,119,w-184,h-231);local onpage=math.min(12,total-(V.page-1)*12);local leftcount=(onpage+1)//2
 local colw=band.w//2;local rowh=math.max(24,band.h//6);local top=band.y+(band.h-leftcount*rowh)//2
 smudge.rect(156,top,3,leftcount*rowh,true,true)
 for slot=0,onpage-1 do
  local i=(V.page-1)*12+slot;local column=slot<leftcount and 0 or 1;local row=slot<leftcount and slot or slot-leftcount
  local x,y=band.x+column*colw,top+row*rowh;local word=tonumber(g.history:sub(i*5+1,i*5+4),16);local mark=tonumber(g.history:sub(i*5+5,i*5+5))
  if mark<3 then F.disc(x+10,y+rowh//2,8,true);if mark==2 then F.disc(x+10,y+rowh//2,6,false)end else smudge.rect(x+2,y+rowh//2-2,16,4,true,true) end
  F.inkText(M.word(word),F.box(x+26,y,colw-30,rowh),14,false,true)
 end
 F.pips(V.hit,w,h-112,pages,V.page,'page')
 for i,label in ipairs({'PLAY AGAIN','DONE'})do local b=F.box(i==1 and 16 or w//2+6,h-68,(w-44)//2,52);F.rect(b,true,false);F.nativeText(label,F.box(b.x+4,b.y+2,b.w-8,b.h-4),14,true);hit(b,i==1 and 'again'or 3)end
end
function V.persist(g)return played.save(g)end
function V.mustPersistBeforeAction(action)return played.needsClear and(action==4 or action=='again')end
function V.draw(g,M,notice,focus)
 played.observe(g)
 if previous~=g.phase then if g.phase==3 then smudge.full_refresh() end;V.focus=1;V.confirmReset=false;if g.phase==0 then V.screen='menu' end;if g.phase==3 then V.page=1 end;previous=g.phase end
 local mode=g.phase==0 and 'portrait' or 'landscape_ccw'
 if orientation~=mode then pcall(smudge.set_orientation,mode);orientation=mode end
 local w,h=smudge.get_bounds();V.hit={};smudge.clear();V.drawnPhase,V.drawnScreen=g.phase,V.screen
 if g.phase==0 then
  if V.screen=='picker' then picker(g,M,w,h) elseif V.screen=='settings' then settings(g,w,h) elseif V.screen=='help' then help(w,h) else menu(g,M,w,h) end
 elseif g.phase==2 then ready(g,M,w,h) elseif g.phase==1 then play(g,M,w,h) else results(g,M,w,h) end
 if V.keyboard and V.hit[V.focus] and g.phase==0 and V.screen~='picker'and V.screen~='help'then F.rect(V.hit[V.focus].box,false,false,3) end
 if notice~='' then F.block(notice,F.box(16,84,w-32,30),14,1) end
end
function V.act(g,M,action,value)
 if V.drawnPhase~=g.phase or V.drawnScreen~=V.screen then return end
 if type(action)=='number' then return M.act(g,action) end
 if action=='picker' then V.screen='picker';V.page=(g.category-1)//9+1
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
