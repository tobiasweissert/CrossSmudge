local C=dofile('cp_core.lua')
local J=dofile('cp_journal.lua')
local V=smudge.get_memory().lua_max_kb>=512 and dofile('presentation.lua')or nil
local G={position=C.start(),history={},cursor=12,mode=1,level=2,notice='',menu=false,menuIndex=1,closed=false}
local journal,dirty
local L={}
local modes={'You: White','You: Black','Two players'}
local function whiteBottom()return G.mode~=2 and(not V or G.mode~=3 or G.position.whiteToMove)end
local function key(p) return C.fen(p):match('^(%S+ %S+ %S+ %S+)') end
local function decode(s)
 local mode,level,fen,history=s:match('^([123]) ([123])\n([^\n]+)\n(.*)$')
 if not mode then return nil end;local p=C.parseFen(fen);if not C.valid(p) then return nil end
 local past={};for f in history:gmatch('[^\n]+') do if #past>=100 then return nil end;local old=C.parseFen(f);if not C.valid(old) then return nil end;past[#past+1]=f end
 -- A checksum is not authentication: validate that undo/repetition history is a
 -- contiguous sequence of legal moves, not merely a collection of valid boards.
 for i=1,#past do
  local before=C.parseFen(past[i]);local target=i==#past and C.fen(p) or past[i+1];local found=false
  for _,m in ipairs(C.legal(before)) do local u=C.make(before,m);local same=C.fen(before)==target;C.unmake(before,m,u);if same then found=true;break end end
  if not found then return nil end
 end
 return {position=p,history=past,mode=tonumber(mode),level=tonumber(level)}
end
local function encode() return G.mode..' '..G.level..'\n'..C.fen(G.position)..'\n'..table.concat(G.history,'\n') end
local function save() if dirty then if journal:write(encode()) then dirty=false else G.notice='Save failed; previous copy retained' end end end
local function refresh() smudge.request_update() end
local function status()
 G.moves=C.legal(G.position);G.over=nil
 if #G.moves==0 then G.over=C.check(G.position) and 'Checkmate' or 'Stalemate'
 else local k=key(G.position);local count=1;local limit=math.min(#G.history,G.position.halfmoveClock)
  for i=#G.history,#G.history-limit+1,-1 do if G.history[i]:match('^(%S+ %S+ %S+ %S+)')==k then count=count+1 end end
  if count>=3 then G.over='Draw: threefold repetition' elseif G.position.halfmoveClock>=100 then G.over='Draw: fifty moves' end
 end
end
local function aiTurn() return G.mode~=3 and G.position.whiteToMove==(G.mode==2) end
local function queue()
 if not G.over and aiTurn() and not G.menu and not G.home then G.job=C.newSearch(G.position,({1,3,4})[G.level],60000);G.notice='Thinking...' end
end
local function newGame(home)
 G.home=home or false;G.hasGame=true;G.menu=false;G.position=C.start();G.history={};G.last=nil;G.selected=nil;G.promotion=nil;G.job=nil;G.hint=nil;dirty=true;status();queue();refresh()
end
local function openSettings(home)
 G.job=nil;G.hint=nil;G.menu=true;G.menuFromHome=home or false;G.home=false
 G.uiMode=G.mode;G.uiLevel=G.level;G.restartPending=false;G.menuIndex=home and 3 or 2;G.selected=nil;refresh()
end
local function closeSettings()
 local home=G.menuFromHome;local restart=G.uiMode and G.uiMode~=G.mode
 if G.uiMode and G.uiMode~=G.mode then G.mode=G.uiMode;dirty=true end
 if G.uiLevel and G.uiLevel~=G.level then G.level=G.uiLevel;dirty=true end
 G.uiMode=nil;G.uiLevel=nil;G.restartPending=false;G.menu=false;G.home=home
 if restart then newGame(home)else queue();refresh()end
end
local function startAction(i)
 local kind=G.hasGame and ({'continue','new','nearby','settings'})[i]or({'new','nearby','settings'})[i]
 if kind=='continue'then G.home=false;queue();refresh()
 elseif kind=='new'then newGame(false)
 elseif kind=='settings'then openSettings(true)elseif kind=='nearby'then G.nearbyUnavailable=true;G.home=false;refresh()end
end
local function play(m)
 G.history[#G.history+1]=C.fen(G.position);if #G.history>100 then table.remove(G.history,1) end
 C.make(G.position,m);G.last=m;G.selected=nil;G.promotion=nil;G.notice=C.moveString(m);dirty=true;status();queue();refresh()
end
local function choose(s)
 if G.job or G.over or G.menu or G.home then return end
 if G.selected then local choices={}
  for _,m in ipairs(G.moves) do if m.from==G.selected and m.to==s then choices[#choices+1]=m end end
  if #choices==1 then play(choices[1]);return elseif #choices>1 then G.promotion=choices;G.promoIndex=1;refresh();return end
 end
 local q=G.position.squares[s];G.selected=q~=0 and (q&8==0)==G.position.whiteToMove and s or nil;refresh()
end
local function geometry()
 if V then L=V.geometry();return L end
 local w,h=smudge.get_bounds();local m=smudge.get_metrics();local y=m.top_padding+m.header_height+8
 local size=math.min((w-24)//8,(h-y-m.button_hints_height-82)//8)
 L={w=w,h=h,x=(w-size*8)//2,y=y,size=size,footer=h-m.button_hints_height,buttons={}}
 return L
end
local function screenSquare(r,c) if not whiteBottom()then return r*8+7-c else return (7-r)*8+c end end
local function menuAction()
 local i=G.menuIndex
 if i==1 then if V then closeSettings()else G.menu=false;queue()end
 elseif i==2 then if V then G.mode=G.uiMode or G.mode;G.level=G.uiLevel or G.level;G.uiMode=nil;G.uiLevel=nil;G.restartPending=false end;newGame(false)
 elseif i==3 then if V then G.uiMode=(G.uiMode or G.mode)==3 and 1 or 3;G.restartPending=G.uiMode~=G.mode else G.mode=G.mode%3+1;dirty=true end
 elseif i==4 then if V then G.uiLevel=(G.uiLevel or G.level)%3+1 else G.level=G.level%3+1;dirty=true end
 elseif i==5 then
  if V and #G.history==0 then refresh();return end
  if #G.history>0 then G.position=C.parseFen(table.remove(G.history));if G.mode~=3 and aiTurn() and #G.history>0 then G.position=C.parseFen(table.remove(G.history)) end;G.last=nil;dirty=true;status() end
  G.menu=false;queue()
 elseif i==8 then if V then if G.uiMode~=3 then G.uiMode=G.uiMode==1 and 2 or 1;G.restartPending=G.uiMode~=G.mode end elseif G.mode~=3 then G.mode=G.mode==1 and 2 or 1;dirty=true end
 elseif i==9 then if V then V.showHints=not V.showHints end
 elseif i==6 then if V then closeSettings();G.home=false;G.job=nil end;if not G.over then G.job=C.newSearch(G.position,2,3000);G.hint=true;G.menu=false;G.notice='Finding hint...' end
 else if V then closeSettings()end;G.job=nil;G.hint=nil;save();G.closed=true;smudge.exit() end
 refresh()
end
function on_init()
 journal,G.saved=J.open('chess',11000,function(s) return decode(s)~=nil end)
 if G.saved then local a=decode(G.saved);G.position=a.position;G.history=a.history;G.mode=a.mode;G.level=a.level;G.saved=nil end
 local old,msg=journal:preserve('game',76);if old and journal.sequence==0 then G.notice=msg end
 if V then V.init();G.home=true;G.hasGame=journal.sequence>0;G.startIndex=1 end;status();geometry();queue()
end
function on_draw()
 local l=geometry();if V then V.draw(G,C,l,screenSquare);return end;smudge.clear();smudge.header('Chess',modes[G.mode]..' / '..({'Easy','Medium','Hard'})[G.level])
 for r=0,7 do for c=0,7 do local s=screenSquare(r,c);local x,y=l.x+c*l.size,l.y+r*l.size;local q=G.position.squares[s]
  smudge.rect(x,y,l.size,l.size,true,false)
  if (r+c)%2==1 then smudge.rect_dither(x,y,l.size,l.size,false) end
  if q~=0 then
   if l.size>=38 then local prefix='sprites/'..(q&8==0 and 'w' or 'b')..('PNBRQK'):sub(q&7,q&7);local dx,dy=x+(l.size-36)//2,y+(l.size-36)//2
    smudge.draw_sprite(dx,dy,36,36,prefix..'_m.raw',true);smudge.draw_sprite(dx,dy,36,36,prefix..'_b.raw',false)
    if q&8~=0 then smudge.draw_sprite(dx,dy,36,36,prefix..'_d.raw',true) end
   else smudge.text(x+l.size//2,y+3,(q&8==0 and 'PNBRQK' or 'pnbrqk'):sub(q&7,q&7),10,true,'center',true) end
  end
  if G.cursor==s then smudge.rect(x+1,y+1,l.size-2,l.size-2,false,2,true) end
  if G.selected==s or (G.last and (G.last.from==s or G.last.to==s)) then smudge.rect(x+4,y+4,l.size-8,l.size-8,false,1,true) end
  if G.selected then for _,m in ipairs(G.moves) do if m.from==G.selected and m.to==s then smudge.circle(x+l.size//2,y+l.size//2,3,true,true);break end end end
 end end
 local sy=l.y+l.size*8+5
 smudge.text(l.w//2,sy,G.over or ((G.position.whiteToMove and 'White' or 'Black')..(C.check(G.position) and ' in check' or ' to move')),10,true,'center',true)
 smudge.text(l.w//2,sy+22,G.notice,10,false,'center',true)
 smudge.button_hints('Menu','Select','Up','Down')
 if G.menu or G.promotion then
  local labels=G.promotion and {'Queen','Rook','Bishop','Knight'} or {'Resume','New game',modes[G.mode],'Level: '..({'Easy','Medium','Hard'})[G.level],'Undo turn','Hint','Exit'}
  local width=math.min(360,l.w-40);local height=#labels*38+16;local x,y=(l.w-width)//2,(l.h-height)//2
  smudge.rect(x,y,width,height,true,false);smudge.rect(x,y,width,height,false,2,true);l.buttons={}
  for i,label in ipairs(labels) do local yy=y+8+(i-1)*38;l.buttons[i]={x=x+8,y=yy,w=width-16,h=36}
   local selected=i==(G.promotion and G.promoIndex or G.menuIndex)
   smudge.rect(x+8,yy,width-16,36,selected,1,true);smudge.text(l.w//2,yy+8,label,10,true,'center',not selected)
  end
 end
end
function on_update()
 if G.closed then return end
 if G.job then if C.stepSearch(G.job,24) then local result=G.job;G.job=nil
  if result.hasMove then if G.hint then G.hint=nil;G.notice='Hint: '..C.moveString(result.best);G.last=result.best else play(result.best) end end;refresh()
 end end
 save()
end
function on_button(b,pressed)
 if not pressed or G.closed then return end
 if V and G.nearbyUnavailable then if b=='back'or b=='confirm'then G.nearbyUnavailable=nil;G.home=true;refresh()end;return end
 if V and G.home then
  if b=='back'then G.job=nil;save();G.closed=true;smudge.exit()
  elseif b=='confirm'then startAction(G.startIndex or 1)
  else local delta=(b=='up'or b=='left'or b=='page_back')and -1 or 1;G.startIndex=((G.startIndex or 1)-1+delta)%(G.hasGame and 4 or 3)+1;refresh()end;return
 end
 if G.menu or G.promotion then local n=G.promotion and 4 or 7;local idx=G.promotion and G.promoIndex or G.menuIndex
  if b=='back' then if G.promotion then G.promotion=nil;queue()elseif V then closeSettings()else G.menu=false;queue()end
  elseif b=='confirm' then if G.promotion then play(G.promotion[idx]) else menuAction() end
  elseif b=='left' or b=='up' or b=='page_back' or b=='right' or b=='down' or b=='page_forward' then
   local delta=(b=='left'or b=='up'or b=='page_back')and -1 or 1
   if V and not G.promotion then local order={1};for _,v in ipairs(V.order or{2,5,3,4,8,9,6,7})do order[#order+1]=v end;local at=1;for i,v in ipairs(order)do if v==idx then at=i end end;idx=order[(at-1+delta)%#order+1]
   else idx=(idx-1+delta)%n+1 end
  end
  if G.promotion then G.promoIndex=idx else G.menuIndex=idx end;refresh();return
 end
 if b=='back' then G.job=nil;G.hint=nil;G.selected=nil;if V then save();G.home=true;G.hasGame=true;G.startIndex=1 else G.menu=true;G.menuIndex=1 end
 elseif b=='confirm' then choose(G.cursor)
 else G.buttonCursor=true;local sign=whiteBottom()and 1 or -1
  if b=='up' or b=='page_back' then G.cursor=(G.cursor-sign)%64 elseif b=='down' or b=='page_forward' then G.cursor=(G.cursor+sign)%64
  elseif b=='left' then G.cursor=(G.cursor+8*sign)%64 elseif b=='right' then G.cursor=(G.cursor-8*sign)%64 end
 end
 refresh()
end
function on_touch(x,y,event)
 if event~='tap' or G.closed then return end
 if V and G.nearbyUnavailable then local b=L.unavailable;if b and x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then G.nearbyUnavailable=nil;G.home=true;refresh()end;return end
 if V and G.home then for i,b in ipairs(L.start or{})do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then startAction(i);return end end;return end
 if V and not G.menu and not G.promotion and(L.start or L.whiteBottom~=whiteBottom())then return end
 if G.menu or G.promotion then for i,b in pairs(L.buttons) do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then if G.promotion then play(G.promotion[i]) else G.menuIndex=i;menuAction() end;return end end;return end
 if V then
  G.buttonCursor=false
  if L.gear and x>=L.gear.x and x<L.gear.x+L.gear.w and y>=0 and y<76 then openSettings(false);return end
  if G.over and x>=16 and x<L.w-16 and y>=L.footer and y<L.footer+52 then newGame(false);return end
  if y>=L.footer then return end
 end
 if y>=L.footer then on_button(({'back','confirm','left','right'})[math.min(4,x*4//L.w+1)],true);return end
 local c,r=(x-L.x)//L.size,(y-L.y)//L.size;if c>=0 and c<8 and r>=0 and r<8 then G.cursor=screenSquare(r,c);choose(G.cursor) end
end
function on_swipe(direction)if G.menu or G.home then on_button(direction,true)end end
function on_exit() G.job=nil;G.hint=nil;save();G.closed=true end
return {core=C,hints=function()return V and V.showHints or false end,state=function() return G end,layout=function() return L end}
