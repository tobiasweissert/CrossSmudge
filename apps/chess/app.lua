local C=dofile('cp_core.lua')
local J=dofile('cp_journal.lua')
local G={position=C.start(),history={},cursor=12,mode=1,level=2,notice='',menu=false,menuIndex=1,closed=false}
local journal,dirty
local L={}
local modes={'You: White','You: Black','Two players'}
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
 if not G.over and aiTurn() and not G.menu then G.job=C.newSearch(G.position,({1,3,4})[G.level],60000);G.notice='Thinking...' end
end
local function play(m)
 G.history[#G.history+1]=C.fen(G.position);if #G.history>100 then table.remove(G.history,1) end
 C.make(G.position,m);G.last=m;G.selected=nil;G.promotion=nil;G.notice=C.moveString(m);dirty=true;status();queue();refresh()
end
local function choose(s)
 if G.job or G.over or G.menu then return end
 if G.selected then local choices={}
  for _,m in ipairs(G.moves) do if m.from==G.selected and m.to==s then choices[#choices+1]=m end end
  if #choices==1 then play(choices[1]);return elseif #choices>1 then G.promotion=choices;G.promoIndex=1;refresh();return end
 end
 local q=G.position.squares[s];G.selected=q~=0 and (q&8==0)==G.position.whiteToMove and s or nil;refresh()
end
local function geometry()
 local w,h=smudge.get_bounds();local m=smudge.get_metrics();local y=m.top_padding+m.header_height+8
 local size=math.min((w-24)//8,(h-y-m.button_hints_height-82)//8)
 L={w=w,h=h,x=(w-size*8)//2,y=y,size=size,footer=h-m.button_hints_height,buttons={}}
 return L
end
local function screenSquare(r,c) if G.mode==2 then return r*8+7-c else return (7-r)*8+c end end
local function menuAction()
 local i=G.menuIndex
 if i==1 then G.menu=false;queue()
 elseif i==2 then G.position=C.start();G.history={};G.last=nil;G.selected=nil;G.menu=false;dirty=true;status();queue()
 elseif i==3 then G.mode=G.mode%3+1;dirty=true
 elseif i==4 then G.level=G.level%3+1;dirty=true
 elseif i==5 then
  if #G.history>0 then G.position=C.parseFen(table.remove(G.history));if G.mode~=3 and aiTurn() and #G.history>0 then G.position=C.parseFen(table.remove(G.history)) end;G.last=nil;dirty=true;status() end
  G.menu=false;queue()
 elseif i==6 then if not G.over then G.job=C.newSearch(G.position,2,3000);G.hint=true;G.menu=false;G.notice='Finding hint...' end
 else save();G.closed=true;smudge.exit() end
 refresh()
end
function on_init()
 journal,G.saved=J.open('chess',11000,function(s) return decode(s)~=nil end)
 if G.saved then local a=decode(G.saved);G.position=a.position;G.history=a.history;G.mode=a.mode;G.level=a.level;G.saved=nil end
 local old,msg=journal:preserve('game',76);if old and journal.sequence==0 then G.notice=msg end
 status();geometry();queue()
end
function on_draw()
 local l=geometry();smudge.clear();smudge.header('Chess',modes[G.mode]..' / '..({'Easy','Medium','Hard'})[G.level])
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
 if G.menu or G.promotion then local n=G.promotion and 4 or 7;local idx=G.promotion and G.promoIndex or G.menuIndex
  if b=='back' then G.promotion=nil;G.menu=false;queue()
  elseif b=='confirm' then if G.promotion then play(G.promotion[idx]) else menuAction() end
  elseif b=='left' or b=='up' or b=='page_back' then idx=(idx-2)%n+1
  elseif b=='right' or b=='down' or b=='page_forward' then idx=idx%n+1 end
  if G.promotion then G.promoIndex=idx else G.menuIndex=idx end;refresh();return
 end
 if b=='back' then G.job=nil;G.hint=nil;G.menu=true;G.menuIndex=1;G.selected=nil
 elseif b=='confirm' then choose(G.cursor)
 else local sign=G.mode==2 and -1 or 1
  if b=='up' or b=='page_back' then G.cursor=(G.cursor-sign)%64 elseif b=='down' or b=='page_forward' then G.cursor=(G.cursor+sign)%64
  elseif b=='left' then G.cursor=(G.cursor+8*sign)%64 elseif b=='right' then G.cursor=(G.cursor-8*sign)%64 end
 end
 refresh()
end
function on_touch(x,y,event)
 if event~='tap' or G.closed then return end
 if G.menu or G.promotion then for i,b in ipairs(L.buttons) do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then if G.promotion then play(G.promotion[i]) else G.menuIndex=i;menuAction() end;return end end;return end
 if y>=L.footer then on_button(({'back','confirm','left','right'})[math.min(4,x*4//L.w+1)],true);return end
 local c,r=(x-L.x)//L.size,(y-L.y)//L.size;if c>=0 and c<8 and r>=0 and r<8 then G.cursor=screenSquare(r,c);choose(G.cursor) end
end
function on_exit() G.job=nil;G.hint=nil;save();G.closed=true end
return {core=C,state=function() return G end,layout=function() return L end}
