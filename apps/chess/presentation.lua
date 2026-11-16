-- Original ChessActivity board geometry, Celtic masks, Toybox chrome and settings.
local F,N=dofile('graphics.lua'),dofile('notation.lua');local P={showHints=true};local pieces={};local cache={};local historyKey
local types={'Pawn','Knight','Bishop','Rook','Queen','King'};local inkLeft={6,2,4,4,1,2};local inkWidth={13,19,16,17,22,20};local counts={8,2,2,2,1,1}
local function b(x,y,w,h)return{x=x,y=y,w=w,h=h}end
function P.init()
 F.init();for i,name in ipairs(types)do pieces[i]={};for _,size in ipairs({24,48})do local suffix=size==24 and'Small'or'';pieces[i][size]={solid=assert(smudge.read_file('art/'..name..'Solid'..suffix..'.raw',300)),light=assert(smudge.read_file('art/'..name..'Light'..suffix..'.raw',300))}end end
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
 if G.over then F.label(G.over:upper(),16,top+height-41,l.w-32,41,20);lines=lines-1 end
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
local function settings(G,l)
 header(l.w,'SETTINGS');l.buttons={}
 local values={[3]=G.mode==3 and'P&P'or(G.mode==1 and'COMPUTER / WHITE'or'COMPUTER / BLACK'),[4]=({'EASY','NORMAL','HARD'})[G.level],[5]=#G.history==0 and'NONE'or'',[8]=G.mode==2 and'BLACK'or'WHITE',[9]=P.showHints and'ON'or'OFF'}
 local labels={[2]='NEW GAME',[3]='OPPONENT',[4]='LEVEL',[5]='TAKE BACK',[6]='FIND HINT',[7]='SAVE AND EXIT',[8]='PLAY AS',[9]='MOVE HINTS'}
 local order=G.mode==3 and{2,5,3,9,6,7}or{2,5,3,4,8,9,6,7};P.order=order
 local rh=math.min(62,(l.h-119-84-(#order-1)*6)//#order);local font=rh<40 and 14 or 20
 for row,i in ipairs(order)do local y=119+(row-1)*(rh+6);local selected=G.menuIndex==i;F.box(16,y,l.w-32,rh,selected,false,1)
  local left=font;if F.width(labels[i],left)>l.w//2-40 then left=14 end;F.text(labels[i],28,y+(rh-(left==20 and 25 or 18))//2,left,selected)
  if values[i]then local right=font;if F.width(values[i],right)>l.w//2-40 then right=14 end;if F.width(values[i],right)>l.w//2-40 then right=10 end;F.text(values[i],l.w-28-F.width(values[i],right),y+(rh-({[10]=13,[14]=18,[20]=25})[right])//2,right,selected)end;l.buttons[i]=b(16,y,l.w-32,rh)end
 local y=l.h-68;F.box(16,y,l.w-32,52,G.menuIndex==1,false,1,20);F.label('BACK TO BOARD',16,y,l.w-32,52,20,G.menuIndex==1);l.buttons[1]=b(16,y,l.w-32,52)
end
local function promotion(G,l)
 header(l.w,'PROMOTE TO');local cw=math.min(360,l.w-32);local x=(l.w-cw)//2;local rh=62;local top=math.max(103,(l.h-rh*4-18)//2);l.buttons={}
 for i,kind in ipairs({5,4,3,2})do local y=top+(i-1)*(rh+6);local selected=G.promoIndex==i;F.box(x,y,cw,rh,false,false,selected and 4 or 1);piece(kind|(G.position.whiteToMove and 0 or 8),x+12,y+7,48);F.label(({'QUEEN','ROOK','BISHOP','KNIGHT'})[i],x+70,y,cw-80,rh,20);l.buttons[i]=b(x,y,cw,rh)end
end
function P.draw(G,C,l,screenSquare)
 smudge.clear();if G.promotion then promotion(G,l);return elseif G.menu then settings(G,l);return end
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
 if G.selected then local r,col;if G.mode==2 then r=G.selected//8;col=7-G.selected%8 else r=7-G.selected//8;col=G.selected%8 end;smudge.rect(l.x+col*l.size-4,l.y+r*l.size-4,l.size+8,l.size+8,false,4,true)end
 smudge.rect(l.x-9,l.y-9,size+18,size+18,false,9,true)
 -- Button-only cursor remains distinct from the selection frame.
 if G.buttonCursor and not G.selected then local r,col;if G.mode==2 then r=G.cursor//8;col=7-G.cursor%8 else r=7-G.cursor//8;col=G.cursor%8 end;corners(l.x+col*l.size+3,l.y+r*l.size+3,l.size-6,10,2)end
 captures(G,l.y-9-12-24,true);captures(G,l.y+size+9+12,false)
 local top=l.y+size+4+12+24+12;moveList(G,C,l,top,l.footer-top-12)
 local report=G.notice:lower():find('fail')or G.notice:find('Old save')or G.notice:find('Hint:')
 local text=G.over or(report and G.notice)or(G.job and(G.hint and'FINDING HINT'or'THINKING...')or((G.position.whiteToMove and'WHITE'or'BLACK')..(C.check(G.position)and' IN CHECK'or' TO MOVE')))
 F.box(16,l.footer,l.w-32,52,G.over~=nil,false,1,20);F.label(text:upper(),16,l.footer,l.w-32,52,20,G.over~=nil)
end
return P
