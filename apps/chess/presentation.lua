-- Original ChessActivity board geometry, Celtic masks, Toybox chrome and settings.
local F,N=dofile('graphics.lua'),dofile('notation.lua');local P={showHints=true};local pieces={};local cache={};local historyKey
local types={'Pawn','Knight','Bishop','Rook','Queen','King'};local inkLeft={6,2,4,4,1,2};local inkWidth={13,19,16,17,22,20};local counts={8,2,2,2,1,1}
local function b(x,y,w,h)return{x=x,y=y,w=w,h=h}end
function P.init()
 F.init();P.nearby=assert(smudge.read_file('art/nearby32.raw',128));for i,name in ipairs(types)do pieces[i]={};for _,size in ipairs({24,48})do local suffix=size==24 and'Small'or'';pieces[i][size]={solid=assert(smudge.read_file('art/'..name..'Solid'..suffix..'.raw',300)),light=assert(smudge.read_file('art/'..name..'Light'..suffix..'.raw',300))}end end
end
function P.geometry()
 local w,h=smudge.get_bounds();local size=math.max(8,math.min(w-50,h-83-72-76-8-12)//8)
 return{w=w,h=h,x=(w-size*8)//2,y=140,size=size,footer=h-16-52,buttons={}}
end
local function piece(q,x,y,size)
 local data=pieces[q&7][size];if q&8==0 then smudge.bitmap(x,y,size,size,data.solid,true);smudge.bitmap(x,y,size,size,data.light)else smudge.bitmap(x,y,size,size,data.solid)end
end
local function header(w,title)
 smudge.rect(0,0,w,76,true,true);F.text(title,16,19,30,true);smudge.rect(0,80,w,3,true,true)
end
local function corners(x,y,size,arm,stroke)
 for i=0,3 do local cx=i&1~=0 and x+size-arm or x;local cy=i&2~=0 and y+size-stroke or y;smudge.rect(cx,cy,arm,stroke,true,true);smudge.rect(i&1~=0 and x+size-stroke or x,i&2~=0 and y+size-arm or y,stroke,arm,true,true)end
end
local function captures(G,y,black)
 local present={0,0,0,0,0,0};for s=0,63 do local q=G.position.squares[s];if q~=0 and(q&8~=0)==black then present[q&7]=present[q&7]+1 end end
 local x=16;for _,kind in ipairs({5,4,3,2,1})do for _=1,counts[kind]-present[kind]do piece(kind|(black and 8 or 0),x-inkLeft[kind],y,24);x=x+inkWidth[kind]+4 end end
end
local function moveList(G,C,l,top,height)
 local lines=height//41;if lines<=0 then return end
 local result=G.over and(G.over:find('threefold')and'DRAW: REPETITION'or G.over:find('fifty')and'DRAW: 50 MOVES'or G.over=='Checkmate'and(G.position.whiteToMove and'BLACK WINS'or'WHITE WINS')or'STALEMATE')
 if result then F.label(result,16,top+height-41,l.w-32,41,20);lines=lines-1 end
 if lines<=0 or #G.history==0 then return end
 local key=G.history[1]..'|'..#G.history..'|'..G.history[#G.history]..'|'..C.fen(G.position);if key~=historyKey then historyKey=key;cache={}end
 local first=math.max(1,#G.history-lines*2+1);local p=C.parseFen(G.history[first]);if not p.whiteToMove and first>1 then first=first-1 end
 local row=0;local i=first
 while row<lines and i<=#G.history do
  local before=C.parseFen(G.history[i]);local text=before.fullmoveNumber..(before.whiteToMove and'. 'or'... ')
  local function notation(index)
   if not cache[index]then cache[index]=N.between(C,G.history[index],G.history[index+1]or C.fen(G.position))end;return cache[index]
  end
  text=text..notation(i);if before.whiteToMove and i<#G.history then text=text..'  '..notation(i+1);i=i+1 end
  F.label(text,16,top+row*41,l.w-32,41,20);row=row+1;i=i+1
 end
end
local function row(label,value,at,selected)
 F.box(at.x,at.y,at.w,at.h,selected,false,1)
 local font=20;F.text(label,at.x+12,at.y+19,font,selected)
 if value and value~=''then F.text(value,at.x+at.w-12-F.width(value,font),at.y+19,font,selected)end
end
local function front(G,l)
 header(l.w,'CHESS');l.start={};l.buttons={};l.gear=nil
 local names=G.hasGame and{'CONTINUE','NEW GAME','PLAY NEARBY','SETTINGS'}or{'NEW GAME','PLAY NEARBY','SETTINGS'}
 local height=#names*62+(#names-1)*6+12;local top=l.h-16-height
 for i,name in ipairs(names)do
  local at=b(16,top+(i-1)*66,l.w-32,62);l.start[i]=at
  local detail=G.hasGame and i==1 and ((#G.history//2+1)..' '..(G.over and'OVER'or G.position.whiteToMove and'WHITE'or'BLACK'))or nil
  row(name,detail,at,(G.startIndex or 1)==i)
  if name=='PLAY NEARBY'then smudge.bitmap(at.x+at.w-56,at.y+15,32,32,P.nearby,(G.startIndex or 1)==i)end
 end
 local slotTop,slotHeight=119,top-119;local pieceSize,cell=48,52
 if cell*8>l.w-32-36 or cell*8>slotHeight-36 then pieceSize,cell=24,32 end
 local side=cell*8
 if l.w-32>=side and slotHeight>=side then
  local x,y=(l.w-side)//2,slotTop+24
  corners(x-12,y-12,side+24,24,4)
  for i=0,63 do local file,rank=i%8,i//8;local xx,yy=x+file*cell,y+(7-rank)*cell
   if(file+rank)%2==0 then smudge.rect_dither(xx,yy,cell,cell,false)end
   local q=G.position.squares[i];if q~=0 then piece(q,xx+(cell-pieceSize)//2,yy+(cell-pieceSize)//2,pieceSize)end
  end
 end
end
local function unavailable(G,l)
 header(l.w,'NEARBY');l.start=nil;l.buttons={}
 F.center('HOST SUPPORT REQUIRED',16,128,l.w-32,52,20)
 local text='NEARBY PLAY IS NOT EXPOSED BY THIS LUA HOST.'
 F.label(text,16,210,l.w-32,l.w>l.h and 66 or 120,l.w>l.h and 10 or 20)
 local at=b(16,l.h-68,l.w-32,52);F.box(at.x,at.y,at.w,at.h,true,false,1);F.label('BACK',at.x,at.y,at.w,at.h,20,true);l.unavailable=at
end
local function settings(G,l)
 header(l.w,'SETTINGS');l.buttons={};l.start=nil
 local mode,level=G.uiMode or G.mode,G.uiLevel or G.level
 local values={[3]=mode==3 and'P&P'or'COMPUTER',[4]=({'EASY','NORMAL','HARD'})[level],[5]=#G.history==0 and'NONE'or'',[8]=mode==2 and'BLACK'or'WHITE',[9]=P.showHints and'ON'or'OFF'}
 local labels={[2]='NEW GAME',[3]='OPPONENT',[4]='LEVEL',[5]='TAKE BACK',[6]='FIND HINT',[7]='SAVE AND EXIT',[8]='PLAY AS',[9]='MOVE HINTS'}
 local order=G.menuFromHome and(mode==3 and{3,9}or{3,4,8,9})or(mode==3 and{2,5,3,9,6,7}or{2,5,3,4,8,9,6,7});P.order=order
 local capacity=math.max(1,(l.h-68-119+6)//68);local selected=1;for i,id in ipairs(order)do if id==G.menuIndex then selected=i end end
 local first=math.max(1,selected-capacity+1);local scrolling=#order>capacity;local rowWidth=l.w-32-(scrolling and 5 or 0)
 for i=first,math.min(#order,first+capacity-1)do local id=order[i];local at=b(16,119+(i-first)*66,rowWidth,62);row(labels[id],values[id],at,G.menuIndex==id);l.buttons[id]=at end
 if scrolling then local track=l.h-68-119;local thumb=math.max(12,track*capacity//#order);local y=119+(track-thumb)*(first-1)//(#order-capacity);smudge.rect_dither(l.w-19,119,3,track,false);smudge.rect(l.w-19,y,3,thumb,true,true)end
 local at=b(16,l.h-68,l.w-32,52);F.box(at.x,at.y,at.w,at.h,true,false,1)
 F.label(G.restartPending and'START NEW GAME'or G.menuFromHome and'BACK TO MENU'or'BACK TO BOARD',at.x,at.y,at.w,at.h,20,true);l.buttons[1]=at
end
local function promotion(G,l)
 header(l.w,'PROMOTE TO');local cw=math.min(360,l.w-32);local x=(l.w-cw)//2;local rh=62;local top=math.max(103,(l.h-rh*4-18)//2);l.buttons={}
 for i,kind in ipairs({5,4,3,2})do local y=top+(i-1)*(rh+6);local selected=G.promoIndex==i;F.box(x,y,cw,rh,false,false,selected and 4 or 1);piece(kind|(G.position.whiteToMove and 0 or 8),x+12,y+7,48);F.label(({'QUEEN','ROOK','BISHOP','KNIGHT'})[i],x+70,y,cw-80,rh,20);l.buttons[i]=b(x,y,cw,rh)end
end
function P.draw(G,C,l,screenSquare)
 smudge.clear();if G.nearbyUnavailable then unavailable(G,l);return end
 l.unavailable=nil;if G.promotion then promotion(G,l);return elseif G.menu then settings(G,l);return elseif G.home then front(G,l);return end
 l.start=nil;l.whiteBottom=G.mode~=2 and(G.mode~=3 or G.position.whiteToMove)
 header(l.w,'CHESS');local gx,gy=l.w-56,18
 -- Toybox.h's procedural ring and eight teeth, in paper on the black band.
 local cx,cy=gx+20,gy+20;local body,tooth=17,6
 for _,d in ipairs({{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}})do local reach=d[1]~=0 and d[2]~=0 and body*7//10 or body;smudge.rounded_rect(cx+d[1]*reach-tooth//2,cy+d[2]*reach-tooth//2,tooth,tooth,1,true,false)end
 smudge.rounded_rect(cx-body,cy-body,body*2,body*2,body,true,false);local hole=body//2;smudge.rounded_rect(cx-hole,cy-hole,hole*2,hole*2,hole,true,true)
 l.gear=b(l.w-68,0,68,76)
 for r=0,7 do for col=0,7 do local s=screenSquare(r,col);local x,y=l.x+col*l.size,l.y+r*l.size;local q=G.position.squares[s]
  if (r+col)%2==1 then smudge.rect_dither(x,y,l.size,l.size,false)end
  if q~=0 then local size=l.size>=48 and 48 or 24;piece(q,x+(l.size-size)//2,y+(l.size-size)//2,size)end
  if P.showHints and G.selected then for _,m in ipairs(G.moves)do if m.from==G.selected and m.to==s then if q==0 then local d=math.max(8,l.size//4);smudge.rounded_rect(x+(l.size-d)//2,y+(l.size-d)//2,d,d,d//2,true,true)else corners(x,y,l.size,math.max(10,l.size//4),4)end;break end end end
 end end
 local size=l.size*8;corners(l.x-17,l.y-17,size+34,34,3)
 if G.selected then local r,col;if G.mode==2 or G.mode==3 and not G.position.whiteToMove then r=G.selected//8;col=7-G.selected%8 else r=7-G.selected//8;col=G.selected%8 end;smudge.rect(l.x+col*l.size-4,l.y+r*l.size-4,l.size+8,l.size+8,false,4,true)end
 smudge.rect(l.x-9,l.y-9,size+18,size+18,false,9,true)
 -- Button-only cursor remains distinct from the selection frame.
 if G.buttonCursor and not G.selected then local r,col;if G.mode==2 or G.mode==3 and not G.position.whiteToMove then r=G.cursor//8;col=7-G.cursor%8 else r=7-G.cursor//8;col=G.cursor%8 end;corners(l.x+col*l.size+3,l.y+r*l.size+3,l.size-6,10,2)end
 captures(G,l.y-9-12-24,true);captures(G,l.y+size+9+12,false)
 local top=l.y+size+4+12+24+12;moveList(G,C,l,top,l.footer-top-12)
 local report=G.notice:lower():find('fail')or G.notice:find('Old save')or G.notice:find('Hint:')
 local text=G.over and'PLAY AGAIN'or(report and G.notice)or(G.job and(G.hint and'FINDING HINT'or'THINKING')or C.check(G.position)and'CHECK'or G.mode==3 and((G.position.whiteToMove and'WHITE'or'BLACK')..' TO MOVE')or'YOUR MOVE')
 F.box(16,l.footer,l.w-32,52,true,false,1);F.label(text:upper(),16,l.footer,l.w-32,52,20,true)
end
return P
