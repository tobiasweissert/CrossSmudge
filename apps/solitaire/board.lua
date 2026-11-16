local U=dofile('design.lua');local V={};local art={}
local suits={'club','diamond','spade','heart'}
function V.init()
 U.init()
 for _,suit in ipairs(suits) do for _,kind in ipairs({'Solid','Outline'}) do for _,size in ipairs({18,46}) do
  local key=suit..kind..'_'..size;art[key]=assert(smudge.read_file('art/'..key..'.raw',300))
 end end end
end
function V.layout(w,h,g,C)
 local gap=w>h and 12 or 7;local cw=math.min(w>h and 92 or 60,(w-16-gap*6)//7);local ch=math.floor(cw*1.42)
 local l={w=w,h=h,cw=cw,ch=ch,x=(w-7*cw-6*gap)//2,gap=gap,top=78,tab=78+ch+17,bottom=h-84,buttons={},pileColumns={[0]=0,[1]=1}}
 local used={};local columns={[0]=6,[1]=5,[2]=3,[3]=4}
 for p=2,5 do if #g.piles[p]>0 then local suit=C.suitOf(g.piles[p][1]);used[suit]=true;l.pileColumns[p]=columns[suit] end end
 for p=2,5 do if not l.pileColumns[p] then for suit=0,3 do if not used[suit] then used[suit]=true;l.pileColumns[p]=columns[suit];break end end end end
 l.menuButton={x=w-180,y=h-70,w=164,h=56}
 return l
end
local function suit(x,y,size,s,outline,gray)
 local key=suits[s+1]..(outline and 'Outline' or 'Solid')..'_'..size
 smudge.bitmap(x,y,size,size,art[key],false,gray or false)
end
function V.card(l,C,x,y,c,selected,focus)
 U.round(x,y,l.cw,l.ch,6,true,true)
 if c and not C.isFaceUp(c) then smudge.rect_dither(x+1,y+1,l.cw-2,l.ch-2,true) end
 smudge.rounded_rect(x,y,l.cw,l.ch,6,false,true,2)
 if selected or focus then smudge.rounded_rect(x+3,y+3,l.cw-6,l.ch-6,4,false,true,2) end
 if not c or not C.isFaceUp(c) then return end
 local rank=({'A','2','3','4','5','6','7','8','9','10','J','Q','K'})[C.rankOf(c)+1]
 local s=C.suitOf(c);local outline=C.isRed(c)
 U.text(rank,x+5,y+16,'b20');suit(x+l.cw-27,y+7,18,s,outline)
 if l.cw>=54 then suit(x+(l.cw-46)//2,y+l.ch-53,46,s,outline) end
end
function V.draw(l,C,g,selected,cursor,index,notice,options)
 smudge.clear();U.header('Solitaire',g.moves..' moves',l.w)
 for p=0,5 do local x=l.x+l.pileColumns[p]*(l.cw+l.gap);local a=g.piles[p]
  if p==0 and #a>0 then V.card(l,C,x,l.top,a[#a],false,cursor==0);U.text(#a,x+l.cw//2,l.top+l.ch//2,'b20','center',true)
  elseif #a>0 then V.card(l,C,x,l.top,a[#a],selected and selected.p==p,cursor==p)
  else
   U.dash(x,l.top,l.cw,l.ch)
   if p>=2 then local bycol={[3]=2,[4]=3,[5]=1,[6]=0};suit(x+(l.cw-46)//2,l.top+(l.ch-46)//2,46,bycol[l.pileColumns[p]],true,true) end
  end
 end
 l.pileSteps={}
 for p=6,12 do local a=g.piles[p];local step=math.max(3,math.min(14,(l.bottom-12-l.tab-l.ch)//math.max(1,#a-1)));l.pileSteps[p]=step
  local x=l.x+(p-6)*(l.cw+l.gap)
  if #a==0 then U.dash(x,l.tab,l.cw,l.ch) end
  for i,c in ipairs(a) do V.card(l,C,x,l.tab+(i-1)*step,c,selected and selected.p==p and i>#a-selected.count,cursor==p and i==index+1) end
 end
 smudge.line(0,l.h-84,l.w-1,l.h-84,true)
 U.text(U.wrap(notice~='' and notice or 'Tap a card to move it',l.menuButton.x-40,'r20',1)[1] or '',24,l.h-40,'r20',nil,false,true)
 U.button('New game',l.menuButton.x,l.menuButton.y,l.menuButton.w,l.menuButton.h,false)
 if options then
  local x,y=l.w//2-150,l.h//2-114
  U.round(x-12,y-12,324,244,12,true,true);U.round(x-12,y-12,324,244,12,false,false)
  for n,label in ipairs({'New draw one','New draw three','Undo','Auto-finish','Exit'}) do
   local b={x=x,y=y+(n-1)*44,w=300,h=40};l.buttons[n]=b;U.button(label,b.x,b.y,b.w,b.h,cursor==-1 and index==n)
  end
 end
end
return V
