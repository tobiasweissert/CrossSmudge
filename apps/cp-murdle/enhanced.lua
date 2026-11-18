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
 local ok,o=pcall(smudge.get_orientation);if ok then original=o end;redraw()
end
local U=dofile('unified_ui.lua');local accusePage=1;local helpPage=1
local function text(t,x,y,width,bold,align)U.text(x,y,U.fit(t,width,'ui10',bold),'ui10',bold,align or'left')end
local function wrap(t,width)
 local out,line={},'';for word in t:gmatch('%S+')do
  local next=line==''and word or line..' '..word
  if smudge.text_width(next,'ui10')>width and line~=''then out[#out+1]=line;line=word else line=next end
 end;if line~=''then out[#out+1]=line end;return out
end
local function footer(labels,actions,values,primary,enabled)
 for i,b in ipairs(U.footer(labels,primary,enabled))do if actions[i]and b.enabled then hit(b,actions[i],values and values[i])end end
end
local function pager(action,current,count)
 local y=h-116;local width=(w-44)//3
 for i,label in ipairs({'<',current..' / '..count,'>'})do
  local b={x=14+(i-1)*(width+8),y=y,w=width,h=40};U.button(label,b)
  if i~=2 then hit(b,action,action=='page'and(i==1 and -1 or 1)or math.max(1,math.min(count,current+(i==1 and -1 or 1))))end
 end
end
local function sheets(p,info)
 local pages={{}};local top=78;local bottom=notice~=''and h-164 or h-128;local y=top
 local function add(lines,index,bold)
  for n,line in ipairs(lines)do
   if y+29>bottom then pages[#pages+1]={};y=top end
   pages[#pages][#pages[#pages]+1]={text=line,y=y,index=index,first=n==1,bold=bold};y=y+29
  end;y=y+10
 end
 if info then
  for cat=1,p.cats do
   add({names[cat]},nil,true)
   for i=1,p.n do add(wrap(p.dossier[(cat-1)*p.n+i]or p.labels[cat][i],w-28),nil)end
  end
 else for i,t in ipairs(p.text)do add(wrap(t,w-76),i)end end
 return pages
end
function on_draw()
 w,h=smudge.get_bounds();hits={};smudge.clear();local p=M.case(g);local m=U.metrics()
 U.header(screen=='settings'and'DIFFICULTY'or screen=='accuse'and'ACCUSE'or screen=='help'and'HOW TO SOLVE'or'MURDLE',screen=='case'and #p.text..' CLUES'or screen=='menu'and tiers[(g.id-1)//16+1]or'')
 if screen=='menu'then
  text(g.phase==1 and'CASE CLOSED'or'THE CASE',14,86,w-28,true)
  text('CASE '..g.id..' / '..tiers[(g.id-1)//16+1],14,128,w-28)
  local count,used=0,0;for b=1,#M.blocks(p)do for r=1,p.n do for c=1,p.n do if M.shown(g,b,r,c)~=0 then count=count+1 end end end end
  for i=1,#p.text do if struck&(1<<(i-1))~=0 then used=used+1 end end
  text(used..' OF '..#p.text..' CLUES USED',14,174,w-28)
  text(count..' OF '..#g.marks..' SQUARES SETTLED',14,214,w-28)
  if h>600 then drawBoard(p,F.box(14,270,w-28,h-370-(notice~=''and 40 or 0)),true)end
  footer({'New case','Level','Help','Play'},{'new','settings','help','play'},nil,4)
 elseif screen=='case'then
  if face==2 then
   drawBoard(p,F.box(14,78,w-28,h-206-(notice~=''and 40 or 0)),false)
   text('Full category names and details: File',14,h-116,w-28)
  else
   local pages=sheets(p,face==3);pageCount=#pages;page=math.max(1,math.min(page,pageCount))
   for _,row in ipairs(pages[page])do
    if row.index then
     local done=struck&(1<<(row.index-1))~=0
     if row.first then local b={x=14,y=row.y,w=30,h=29};U.panel(b,done);U.text(29,row.y+2,tostring(row.index),'ui10',true,'center',not done)end
     text(row.text,58,row.y,w-72);hit({x=14,y=row.y,w=w-28,h=29},'clue',row.index)
    else text(row.text,14,row.y,w-28,row.bold)end
   end
   if pageCount>1 then pager('page',page,pageCount)end
  end
  footer({face==2 and'Clues'or'Grid',face==3 and'Clues'or'File',g.phase==1 and'Solved'or'Accuse'},{'face','face',g.phase==0 and'accuse'or nil},{face==2 and 1 or 2,face==3 and 1 or 3},3)
 elseif screen=='settings'then
  for i=1,4 do local b={x=14,y=78+(i-1)*64,w=w-28,h=56};U.button(tiers[i],b,i==nextTier);hit(b,'tier',i)end
  text('Applies to the next case; current case is kept.',14,notice~=''and 286 or 344,w-28)
  footer({'Back'},{'menu'})
 elseif screen=='confirm'then
  text('DROP THIS CASE?',14,90,w-28,true)
  local y=148;for _,t in ipairs(wrap('All its marks go with it. There is only ever one open case.',w-28))do text(t,14,y,w-28);y=y+29 end
  footer({'Keep it','Start a new one'},{'menu','fresh'},nil,2)
 elseif screen=='accuse'then
  accusePage=math.max(1,math.min(p.cats,accusePage));local cat=accusePage
  text(names[cat],14,80,w-28,true)
  local cw=(w-36)//2
  for i=1,p.n do local b={x=14+(i-1)%2*(cw+8),y=128+(i-1)//2*72,w=cw,h=64};U.button(p.labels[cat][i],b,made[cat]and g.picks[cat]==i);hit(b,'pick',{cat,i})end
  pager('accuse-page',cat,p.cats)
  local complete=true;for c=1,p.cats do if not made[c]then complete=false end end
  footer({'Clues','Submit'},{'clues','submit'},nil,2,{true,complete})
 elseif screen=='verdict'then
  text(g.phase==1 and'CASE CLOSED'or'THEY WALK FREE',14,80,w-28,true)
  local t='It was '..p.labels[1][g.picks[1]]..' with '..phrases[1][p.labels[2][g.picks[2]]]..' '..phrases[2][p.labels[3][g.picks[3]]]
  if p.cats==4 then t=t..', driven by '..phrases[3][p.labels[4][g.picks[4]]]end
  F.block(t,F.box(14,128,w-28,h-264-(notice~=''and 40 or 0)),20,12)
  text(g.phase==1 and'SOLVED / ATTEMPTS '..g.attempts or'THE EVIDENCE DOES NOT SUPPORT IT',14,h-106,w-28)
  footer({'Done',g.phase==1 and'New case'or'Keep looking'},{'menu',g.phase==1 and'new'or'play'},nil,2)
 elseif screen=='help'then
  local beat=beats[helpPage];text(beat[1],14,78,w-28,true)
  F.block(beat[2],F.box(14,128,w-28,h-274-(notice~=''and 40 or 0)),20,18)
  pager('help-page',helpPage,#beats);footer({'Back'},{'menu'})
 end
 if notice~=''then text(notice,14,h-154,w-28,true)end
 if keyboard then focus=math.max(1,math.min(focus,#hits));if hits[focus]then local b=hits[focus].box;smudge.rect(b.x,b.y,b.w,b.h,false,3,true)end end
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
 elseif action=='accuse' then screen='accuse';accusePage=1
 elseif action=='accuse-page'then accusePage=value
 elseif action=='help-page'then helpPage=value
 elseif action=='clues'then screen='case';face=1;page=1
 elseif action=='pick' then g.picks[value[1]]=value[2];made[value[1]]=true
 elseif action=='submit' then local complete=true;for cat=1,M.case(g).cats do if not made[cat]then complete=false end end;if complete then M.accuse(g);screen='verdict'end end
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
