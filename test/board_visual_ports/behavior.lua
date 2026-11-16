local app=assert(os.getenv('BOARD_APP'))
local w,h=smudge.get_bounds();local cap=smudge.get_memory().lua_max_kb
if cap<512 then
 assert(loadfile('main.lua'));dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();on_exit()
 print('Constrained legacy dispatch passed');return
end
local core=dofile('core.lua');local B=dofile('board.lua');local F=dofile('graphics.lua')
dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local expected=core.new(smudge.millis())
local function tap(x,y)on_touch(x,y,'tap');on_draw()end
local function cell(l,col,row)return l.x+(col-.5)*l.cell,l.y+(row-.5)*l.cell end
if app=='connectfour' then
 local l=B.layout(w,h);tap(l.x+3.5*l.cell,l.y+l.lip+2*l.cell);assert(core.action(expected,4))
elseif app=='checkers' then
 local l=B.layout(w,h);local move=core.moves(expected)[1];local from,to=move:byte(1,2)
 tap(cell(l,(from-1)%8+1,(from-1)//8+1));tap(cell(l,(to-1)%8+1,(to-1)//8+1));assert(core.play(expected,move))
elseif app=='knucklebones' then
 local l=B.layout(w,h);tap(l.x+l.cell//2,l.yours+l.cell//2);assert(core.action(expected,1))
elseif app=='hex' then
 local l=B.layout(w,h);local col,row=5,5
 tap(l.x+2*l.a+3*l.a*col,l.y+l.half+2*l.half*row+l.half*col);assert(core.action(expected,6,6))
elseif app=='battleship' then
 for row=1,5 do local l=B.layout(w,h,expected);tap(cell(l,1,row));assert(core.action(expected,1,row))end
 local l=B.layout(w,h,expected);tap(cell(l,1,1));on_button('back',true)
 local untouched=dofile('storage.lua').load(function(raw)local _,body=raw:match('^([12])|(.*)$');return body and core.decode(body)end)
 assert(untouched and core.encode(untouched)==core.encode(expected),'aiming fired before confirmation')
 on_button('confirm',true);on_draw();tap(l.view.x+10,l.view.y+10);assert(core.fire(expected,1))
elseif app=='yahtzee' then
 local l=B.layout(w,h);tap(l.roll.x+10,l.roll.y+10);assert(core.roll(expected))
 local die=l.dice[1];tap(die.x+die.w//2,die.y+die.h//2);assert(core.hold(expected,1))
end
on_touch(-1,-1,'tap');on_touch(w,h,'tap');on_touch(0,0,'move');on_exit()
local S=dofile('storage.lua');local loaded=S.load(function(raw)
 local mode,data=raw:match('^([12])|(.*)$');if mode then return core.decode(data)end
end)
assert(loaded and core.encode(loaded)==core.encode(expected),'visible touch targets disagree with game actions: '..app)
print('Rendered board touch / saved core state / invalid edges passed: '..app)
if app=='battleship' then
 -- Enemy unshot fleet identities must not influence any visible draw command.
 local trace={};local originals={}
 for _,name in ipairs({'rect','rect_dither','rounded_rect','circle','line','bitmap'})do
  originals[name]=smudge[name];smudge[name]=function(...)
   local parts={name};for _,v in ipairs({...})do parts[#parts+1]=tostring(v)end;trace[#trace+1]=table.concat(parts,'|')
  end
 end
 F.init();B.draw(core,expected,1,1,nil,F,w,h);local first=table.concat(trace,'\n')
 local replacement=core.new(98765).b
 for i=1,100 do if expected.sb:sub(i,i)=='1' then replacement=replacement:sub(1,i-1)..expected.b:sub(i,i)..replacement:sub(i+1)end end
 expected.b=replacement;trace={};B.draw(core,expected,1,1,nil,F,w,h)
 assert(first==table.concat(trace,'\n'),'unshot enemy fleet leaked into the screen')
 for name,fn in pairs(originals)do smudge[name]=fn end
 print('Battleship hidden-fleet rendering invariant passed')
end

if B.lesson then
 F.init();local pages=app=='yahtzee' and 4 or 3
 for page=1,pages do local lesson=B.lesson(page,F,w,h);assert(lesson.page==page and lesson.pages==pages)end
 local V=dofile('presentation.lua');V.init()
 local items={'Resume','New solo game','New two-player game','How to play','Save and exit'}
 for page=1,pages do
  V.draw(core,expected,1,'help',1,1,'',nil,items)
  local result=V.button('help','confirm');assert(result==(page==pages and 'done' or true))
 end
 print('Original illustrated help pages and final exit passed')
end

local finished=core.new(1)
if app=='yahtzee'then for n=1,26 do assert(core.roll(finished));assert(core.take(finished,core.choose_box(finished.cards[finished.turn],finished.d)))end
elseif app=='connectfour'then for _,c in ipairs({4,3,4,3,4,2,4})do assert(core.action(finished,c))end
elseif app=='hex'then for r=0,10 do finished.b[r*11+6]=1 end;finished.status=1;local chain=B.chain(finished);local n=0;for _ in pairs(chain)do n=n+1 end;assert(n==11,'winning chain geometry mismatch')
else if app=='battleship'then for row=1,5 do assert(core.action(finished,1,row))end end;finished.status=1 end
F.init();local result=B.draw(core,finished,1,1,nil,F,w,h)
if result.again then assert(B.hit(result,result.again.x+10,result.again.y+10,F)=='new');assert(B.hit(result,result.done.x+10,result.done.y+10,F)=='back')else assert(B.hit(result,result.view.x+10,result.view.y+10,F)=='new')end
print('Result rendering / winning-chain / Again-Done controls passed')

local V=dofile('presentation.lua');V.init();local items=app=='battleship'and {'Resume','Rotate','View','New game','Help','Save and exit'}or{'Resume','New solo','New shared','Help','Save and exit'}
V.draw(core,expected,1,'menu',1,1,'',nil,items);local kind,id=V.button('menu','confirm');assert(kind=='activate'and id==1)
V.button('menu','down');assert(V.button('menu','confirm')==true);V.draw(core,expected,1,'menu',1,1,'',nil,items)
kind,id=V.button('menu','confirm');assert(kind=='activate'and id==(app=='battleship'and 4 or 2),'settings remapped an action')
assert(V.button('menu','back')==true);V.button('menu','down');V.button('menu','down');V.draw(core,expected,1,'menu',1,1,'',nil,items)
kind,id=V.button('menu','confirm');assert(kind=='activate'and id==#items-1,'help front-door mapping failed')
print('Front door / settings / help action mapping passed')

if app=='connectfour'then
 local one=core.new(1);assert(core.action(one,4));local ring=F.ring;local stoneY
 F.ring=function(x,y,...)stoneY=y;return ring(x,y,...)end
 local area={x=16,y=145,w=448,h=400};B.preview(core,one,F,area);F.ring=ring
 assert(stoneY and stoneY>area.y+area.h//2,'miniature floated the first disc at the top')
end
