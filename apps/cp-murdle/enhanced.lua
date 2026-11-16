local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_enhanced.lua')end
-- Original CrossPlay three-face casebook. Tested case/rule/save modules stay unchanged.
local M=dofile('core.lua');local S=dofile('storage.lua');local F=dofile('graphics.lua');local phrases=dofile('phrases.lua')
local g;local screen='menu';local face=1;local page=1;local nextTier=1;local struck=0;local made={};local hits={};local focus=1;local keyboard=false
local notice='';local savedAt=0;local closed=false;local original;local w,h=480,800;local art={};local pageCount=1
local tiers={'ELEMENTARY','NOSY','HARD BOILED','IMPOSSIBLE'}
local shapes={'3 OF EACH, PLAIN CLUES','4 OF EACH, NOTHING SAID PLAINLY','4 OF EACH PLUS MOTIVES','MOTIVES, AND THE KILLER LIES'}
local names={'THE SUSPECTS','WEAPONS','PLACES','MOTIVES'}
local icons={'suspects','weapons','places','motives'}
local faceIcons={'face_clues','face_grid','face_info'}
local function save()local ok=S.save(M.encode(g));if not ok then notice='SAVE FAILED - RETRY' end;return ok end
local function redraw()smudge.request_update()end
local function hit(b,action,value)F.hit(hits,b,action,value);return b end
local function button(label,b,action,value,outline)
 F.button(label,b,not outline,20)
 if action then hit(b,action,value) end
end
local function icon(cat,x,y,paper)smudge.bitmap(x,y,24,24,art[icons[cat]],paper or false)end
local function paragraph(text,x,y,width,size)
 local lines=F.wrap(text,width,size);local step=size==10 and 21 or 42
 for _,line in ipairs(lines) do F.nativeText(line,F.box(x,y,width,step),size,false,true);y=y+step end;return y
end
local function blockIndex(p,a,b)
 if a>b then a,b=b,a end
 for i,pair in ipairs(M.blocks(p)) do if pair[1]==a and pair[2]==b then return i end end
end
local function drawBoard(p,b,mini)
 local groups=p.cats-1;local span=groups*p.n;local gutter=mini and 0 or 51;local head=mini and 0 or 51
 local cell=math.min((b.w-gutter)//span,(b.h-head)//span);cell=math.max(6,cell)
 local x0=b.x+(b.w-gutter-cell*span)//2+gutter;local y0=b.y+head
 if mini then y0=b.y+(b.h-cell*span)//2 end
 local columns={1};for i=2,groups do columns[i]=p.cats-i+2 end
 local rows={};for i=1,groups do rows[i]=i+1 end
 if not mini then
  for col,cat in ipairs(columns) do
   icon(cat,x0+(col-1)*p.n*cell+(p.n*cell-24)//2,y0-head)
   for i=1,p.n do F.nativeText(p.labels[cat][i]:sub(1,1),F.box(x0+((col-1)*p.n+i-1)*cell,y0-23,cell,21),10) end
  end
  for row,cat in ipairs(rows) do
   icon(cat,x0-gutter,y0+(row-1)*p.n*cell+(p.n*cell-24)//2)
   for i=1,p.n do F.nativeText(p.labels[cat][i]:sub(1,1),F.box(x0-gutter+26,y0+((row-1)*p.n+i-1)*cell+(cell-21)//2,gutter-30,21),10) end
  end
 end
 for row,a in ipairs(rows) do for col,bcat in ipairs(columns) do if row+col<=groups+1 then
  local block=blockIndex(p,a,bcat);local bx=x0+(col-1)*p.n*cell;local by=y0+(row-1)*p.n*cell
  for r=1,p.n do for c=1,p.n do
   local br,bc=r,c;if a>bcat then br,bc=c,r end
   local box=F.box(bx+(c-1)*cell,by+(r-1)*cell,cell,cell);F.rect(box,false,false,1)
   local mark=M.shown(g,block,br,bc);local raw=g.marks[(block-1)*p.n*p.n+(br-1)*p.n+bc]
   if mark==1 then
    local barW=mini and cell//3 or cell*5//9;local barH=mini and 2 or (raw==0 and 2 or 4)
    F.rect(F.box(box.x+(mini and cell//3 or(cell-barW)//2),box.y+cell//2-barH//2,barW,barH),true)
   elseif mark==2 then local inset=cell//4;if mini then F.rect(F.box(box.x+inset,box.y+inset,cell//2,cell//2),true)else F.round(F.box(box.x+inset,box.y+inset,cell-inset*2,cell-inset*2),3,true)end end
   if not mini and g.phase==0 then hit(box,'cell',{block,br,bc}) end
  end end
  F.rect(F.box(bx,by,p.n*cell,p.n*cell),false,false,3)
 end end end
 return y0+span*cell
end
local function drawLegend(p,y,bottom)
 local pitch=math.min(52,(bottom-y)//p.cats);local rowH=math.max(24,pitch);y=math.max(y,y+(bottom-y-rowH*p.cats)//2)
 local widest=0;for cat=1,p.cats do for i=1,p.n do widest=math.max(widest,F.inkWidth(p.labels[cat][i]:sub(1,1)..'='..p.labels[cat][i],10))end end
 local colW=math.min(widest+12,(w-66)//p.n)
 for cat=1,p.cats do
  icon(cat,16,y+(rowH-24)//2)
  for i=1,p.n do F.nativeText(p.labels[cat][i]:sub(1,1)..'='..p.labels[cat][i],F.box(50+(i-1)*colW,y+(rowH-21)//2,colW,21),10,false,true)end
  y=y+rowH
 end
end
local function textPages(p,info)
 local pages={{}};local y=119;local bottom=h-136
 if info then
  for cat=1,p.cats do
   local want=23+6+16+p.n*23
   if y+want>bottom and #pages[#pages]>0 then pages[#pages+1]={};y=119 end
   local divider=#pages[#pages]>0
   if divider then y=y+16 end
   local lines={}
   for i=1,p.n do local entry=p.dossier[(cat-1)*p.n+i]or p.labels[cat][i];local detail=entry:match(':%s*(.*)$')or''
    lines[#lines+1]=p.labels[cat][i]:sub(1,1)..'  '..p.labels[cat][i]..(detail~=''and('  ('..(cat==1 and''or'with ')..detail..')')or'')end
   pages[#pages][#pages[#pages]+1]={category=cat,lines=lines,y=y,divider=divider};y=y+27+p.n*23
  end
 else
  for i,text in ipairs(p.text)do local lines=F.wrap(text,w-64,10);local height=#lines*21
   if y+height>bottom and #pages[#pages]>0 then pages[#pages+1]={};y=119 end
   pages[#pages][#pages[#pages]+1]={index=i,lines=lines,y=y,h=height};y=y+height+10
  end
 end
 return pages
end
local beats={
 {'EVERYONE HAS A PLACE','Each suspect carried one weapon and was in one place, and no two of them share. One of them did it.'},
 {'THE GRID IS YOUR PENCIL','Every pair of things meets in one square. Tap once to rule it out, again to lock it in. Locking one rules out the rest of its row and column in a lighter mark. The grid only ever takes back marks it made itself; yours stay until you clear them.'},
 {'THE CLUES DO THE REST','Read one, mark what it rules out, tap its number to tick it off. Clues never lie, unless the case says a suspect is speaking.'},
 {'THEN NAME THEM','The last clue describes what was beside the body, not who left it there. Find that thing on your grid.'}}
function on_init()
 closed=false;g=S.load(M.decode) or M.new(smudge.millis());M.pause(g);screen='menu';face=1;struck=0;made={};nextTier=(g.id-1)//16+1
 F.init();for _,name in ipairs({'suspects','weapons','places','motives','face_clues','face_grid','face_info'}) do art[name]=assert(smudge.read_file('art/'..name..'.raw',72)) end
 local ok,o=pcall(smudge.get_orientation);if ok then original=o end;pcall(smudge.set_orientation,'portrait');redraw()
end
function on_draw()
 w,h=smudge.get_bounds();hits={};smudge.clear();local p=M.case(g);local footer=F.box(16,h-78,w-32,62)
 local title=screen=='settings' and 'DIFFICULTY' or screen=='accuse' and 'ACCUSE' or screen=='help' and 'HOW TO SOLVE' or 'MURDLE'
 F.header(title,w,screen=='menu' and tiers[(g.id-1)//16+1] or nil)
 if screen=='menu' then
  F.nativeText(g.phase==1 and 'CASE CLOSED' or 'THE CASE',F.box(16,119,w-32,63),30,false,true)
  F.nativeText('CASE '..g.id..', '..tiers[(g.id-1)//16+1]..(g.phase==1 and', CLOSED'or''),F.box(16,184,w-32,21),10,false,true)
  local board=F.box(16,217,w-32,h-217-266);drawBoard(p,board,true);hit(F.box(16,119,w-32,h-119-250),'play')
  local count,used=0,0;for b=1,#M.blocks(p) do for r=1,p.n do for c=1,p.n do if M.shown(g,b,r,c)~=0 then count=count+1 end end end end
  for i=1,#p.text do if struck&(1<<(i-1))~=0 then used=used+1 end end
  F.nativeText(used..' OF '..#p.text..' CLUES USED',F.box(16,h-260,w-32,21),10)
  F.nativeText(count..' OF '..#g.marks..' SQUARES SETTLED',F.box(16,h-239,w-32,21),10)
  button('NEW CASE',F.box(16,h-210,w-32,62),'new')
  button('DIFFICULTY',F.box(16,h-144,w-32,62),'settings',nil,true)
  button('HOW TO SOLVE',footer,'help',nil,true)
 elseif screen=='case' then
  local door=0;for f=1,3 do if f~=face then local x=w-112+door*56;smudge.bitmap(x+16,26,24,24,art[faceIcons[f]],true);hit(F.box(x,0,56,76),'face',f);door=door+1 end end
  if face==2 then
   local bottom=drawBoard(p,F.box(16,119,w-32,h-119-140),false)
   if notice~=''then F.nativeText(notice,F.box(16,bottom+8,w-32,21),10)end
   drawLegend(p,bottom+37,h-82)
  else
   local sheets=textPages(p,face==3);pageCount=#sheets;page=math.max(1,math.min(page,pageCount))
   for _,item in ipairs(sheets[page]) do
    if item.category then
     if item.divider then smudge.rect(16,item.y-10,w-32,1,true,true)end
     F.nativeText(names[item.category],F.box(16,item.y,w-32,23),10,false,true);local y=item.y+27
     for _,line in ipairs(item.lines)do F.nativeText(line,F.box(28,y,w-44,23),10,false,true);y=y+23 end
    else
     local done=(struck&(1<<(item.index-1)))~=0;local tick=F.box(16,item.y,24,23)
     F.round(tick,5,done,false,1);F.nativeText(item.index,F.box(tick.x,tick.y+1,tick.w,21),10,done)
     local y=item.y;for _,line in ipairs(item.lines) do F.nativeText(line,F.box(48,y,w-64,21),10,false,true);y=y+21 end
     hit(F.box(16,item.y,w-32,item.h+10),'clue',item.index)
    end
   end
   if pageCount>1 then
    for i,label in ipairs({'<','>'})do local live=i==1 and page>1 or i==2 and page<pageCount;local b=F.box(i==1 and 16 or w-64,h-128,48,40)
     if live then F.round(b,8,false,false,1);F.nativeText(label,F.box(b.x+4,b.y+2,b.w-8,b.h-4),20);hit(b,'page',i==1 and -1 or 1)else F.disabled(b,label,20,8)end end
    local used=0;for i=1,#p.text do if struck&(1<<(i-1))~=0 then used=used+1 end end;F.nativeText(page..' / '..pageCount..'      '..used..' OF '..#p.text..' DONE',F.box(16,h-118,w-32,22),10)
   end
  end
  button(g.phase==1 and 'SOLVED' or 'ACCUSE',footer,g.phase==0 and 'accuse' or nil)
 elseif screen=='settings' then
  for i=1,4 do local b=F.box(16,119+(i-1)*86,w-32,76);local selected=i==nextTier
   F.round(b,10,selected,false,1);F.nativeText(tiers[i],F.box(28,b.y+6,w-56,42),20,selected,true);F.nativeText(shapes[i],F.box(28,b.y+48,w-56,21),10,selected,true);hit(b,'tier',i)
  end
  paragraph('The case you have open keeps the size it was made at. This applies to the next one.',16,479,w-32,10)
 elseif screen=='confirm' then
  F.nativeText('DROP THIS CASE?',F.box(16,119,w-32,56),30,false,true,true)
  paragraph('The marks you have made will go with it. There is only ever one case open at a time.',16,189,w-32,10)
  button('START A NEW ONE',F.box(16,h-144,w-32,62),'fresh')
  button('KEEP IT',footer,'menu',nil,true)
 elseif screen=='accuse' then
  local y=119;local rowH=40;local gap=8;local cw=(w-40)//2
  for cat=1,p.cats do
   F.nativeText(cat==1 and'SUSPECTS'or names[cat],F.box(16,y,w-32,25),10,false,true);y=y+25
   for i=1,p.n do local b=F.box(16+(i-1)%2*(cw+8),y+(i-1)//2*(rowH+gap),cw,rowH);local selected=made[cat] and g.picks[cat]==i
    F.round(b,8,selected,false,1);F.nativeText(p.labels[cat][i],F.box(b.x+4,b.y+2,b.w-8,b.h-4),10,selected);hit(b,'pick',{cat,i})
   end
   y=y+((p.n+1)//2)*(rowH+gap)+10
  end
  local complete=true;for cat=1,p.cats do if not made[cat] then complete=false end end
  if complete then button('THAT IS MY ACCUSATION',footer,'submit')else F.disabled(footer,'THAT IS MY ACCUSATION',20)end
 elseif screen=='verdict' then
  F.nativeText(g.phase==1 and 'CASE CLOSED' or 'THEY WALK FREE',F.box(16,119,w-32,60),30)
  local text='It was '..p.labels[1][g.picks[1]]..' with '..phrases[1][p.labels[2][g.picks[2]]]..' '..phrases[2][p.labels[3][g.picks[3]]]
  if p.cats==4 then text=text..', driven by '..phrases[3][p.labels[4][g.picks[4]]]end
  local y=paragraph(text..'.',16,199,w-32,20)
  paragraph(g.phase==1 and (g.attempts==1 and 'SOLVED, FIRST TIME OF ASKING.' or 'SOLVED, AFTER '..(g.attempts-1)..' WRONG ACCUSATIONS.') or 'THE EVIDENCE DOES NOT SUPPORT IT.',16,y+14,w-32,10)
  local b=F.box(16,h-144,w-32,62);F.button(g.phase==1 and 'ANOTHER CASE'or'KEEP LOOKING',b,true,30);hit(b,g.phase==1 and'new'or'play')
  button('DONE',footer,'menu',nil,true)
 elseif screen=='help' then
  local y=119
  for i,beat in ipairs(beats) do
   F.nativeText(beat[1],F.box(16,y,w-32,42),20,false,true);y=paragraph(beat[2],16,y+44,w-32,10)
   if i==2 then
    y=y+10;local labels={'NOT SURE','IT WAS THEM','YOU RULED OUT','GRID RULED OUT'}
    for n,label in ipairs(labels) do local x=16+(n-1)%2*((w-32)//2);local top=y+(n-1)//2*50
     F.rect(F.box(x,top,40,40),false)
     if n==2 then F.round(F.box(x+10,top+10,20,20),3,true) elseif n>=3 then smudge.rect(x+9,top+20-(n==3 and 2 or 1),22,n==3 and 4 or 2,true,true) end
     F.nativeText(label,F.box(x+48,top+9,(w-32)//2-52,21),10,false,true)
    end;y=y+106
   end;y=y+16
  end
  hit(F.box(16,119,w-32,h-135),'menu')
 end
 if keyboard then focus=math.max(1,math.min(focus,#hits));if hits[focus] then local b=hits[focus].box;F.corners(b,math.min(8,b.w),2) end end
 if notice=='SAVE FAILED - RETRY' then F.center(notice,F.box(16,h-105,w-32,21),10) end
end
local function dispatch(action,value)
 if action=='play' then screen='case'
 elseif action=='menu' then screen='menu'
 elseif action=='new' then screen='confirm'
 elseif action=='fresh' then
  local id=(nextTier-1)*16+((g.id-1)%16+1)%16+1;g=M.new(g.seed,id);struck=0;made={};face=1;page=1;screen='case';save()
 elseif action=='settings' then screen='settings'
 elseif action=='tier' then nextTier=value
 elseif action=='help' then screen='help'
 elseif action=='face' then face=value;page=1
 elseif action=='page' then page=math.max(1,math.min(pageCount,page+value))
 elseif action=='clue' then struck=struck~(1<<(value-1))
 elseif action=='cell' then notice=M.tap(g,value[1],value[2],value[3]) and '' or 'ALREADY TICKED ELSEWHERE'
 elseif action=='accuse' then screen='accuse'
 elseif action=='pick' then g.picks[value[1]]=value[2];made[value[1]]=true
 elseif action=='submit' then M.accuse(g);screen='verdict' end
 focus=1;hits={} -- Retire the old screen targets until its replacement is drawn.
 if smudge.millis()-savedAt>=5000 then save();savedAt=smudge.millis() end
 redraw()
end
function on_button(btn,pressed)
 if not pressed or closed then return end
 if btn=='back' then
  if screen=='menu' then if save() then closed=true;smudge.exit() end
  elseif screen=='accuse' then screen='case'
  else screen='menu' end;focus=1;hits={};redraw();return
 end
 if screen=='case' and face~=2 and (btn=='page_back' or btn=='page_forward') then dispatch('page',btn=='page_forward' and 1 or -1);return end
 if btn=='confirm' then local item=hits[math.max(1,math.min(focus,#hits))];if item then dispatch(item.action,item.value) end
 else keyboard=true;if btn=='up' or btn=='left' or btn=='page_back' then focus=(focus-2)%math.max(1,#hits)+1 else focus=focus%math.max(1,#hits)+1 end;redraw() end
end
function on_touch(x,y,event)
 if event~='tap' or closed or x<0 or y<0 or x>=w or y>=h then return end
 local item=F.at(hits,x,y);if item then keyboard=false;dispatch(item.action,item.value) end
end
function on_swipe(dir)
 if closed then return end
 if screen=='case' and face~=2 then dispatch('page',(dir=='up' or dir=='left') and 1 or -1) else on_button(dir,true) end
end
function on_update()end
function on_exit()M.pause(g);save();closed=true;if original then pcall(smudge.set_orientation,original) end end
return {state=function()return g end,core=M,hits=function()return hits end,screen=function()return screen end,struck=function()return struck end,view=function()return {face=face,page=page,nextTier=nextTier,made=made,notice=notice}end}
