local F=dofile('graphics.lua');local V={};local art={}
V.graphics=F
local suits={'club','diamond','spade','heart'}
function V.init()
 F.init()
 for _,suit in ipairs(suits) do for _,kind in ipairs({'Solid','Outline'}) do for _,size in ipairs({18,46}) do
  local key=suit..kind..'_'..size;art[key]=assert(smudge.read_file('art/'..key..'.raw',300))
 end end end
end
function V.layout(w,h,g,C)
 local gap=w>h and 20 or 7;local cw=w>h and 92 or math.min(60,(w-32-gap*6)//7);local ch=w>h and 122 or cw*122//92
 local l={w=w,h=h,cw=cw,ch=ch,x=w>h and 16 or (w-7*cw-6*gap)//2,gap=gap,top=75,tab=75+ch+24,bottom=h-16,buttons={},pileColumns={[0]=0,[1]=1}}
 for p=2,5 do l.pileColumns[p]=p+1 end
 l.wasteFan=w>h and 36 or 18
 l.waste={x=l.x+cw+gap,y=l.top,w=cw+math.max(0,math.min(3,#g.piles[1])-1)*l.wasteFan,h=ch}
 l.menuButton={x=w-116,y=8,w=100,h=40}
 return l
end
local function suit(x,y,size,s,outline,gray)
 local key=suits[s+1]..(outline and 'Outline' or 'Solid')..'_'..size
 smudge.bitmap(x,y,size,size,art[key],false,gray or false)
end
function V.pip(x,y,s) suit(x,y+1,18,s,s%2==1)end
function V.confirm(w,h,three)
 local y=h//2-55
 smudge.rect(24,y,w-48,110,true,false);smudge.rect(24,y,w-48,110,false,true,2)
 F.center(three and 'DEAL FRESH THREE?'or'DEAL FRESH ONE?',24,y+8,w-48,42,20)
 F.center('BACK: CANCEL   CONFIRM: DEAL',24,y+58,w-48,28,10)
end
function V.card(l,C,x,y,c,selected,focus,visible,sideways)
 visible=visible or l.ch
 smudge.rounded_rect(x,y,l.cw,l.ch,8,true,false)
 smudge.rounded_rect(x,y,l.cw,l.ch,8,false,true,2)
 if not c then return end
 if not C.isFaceUp(c) then
  F.roundDither(x+7,y+7,l.cw-14,l.ch-14,4,true)
  smudge.rounded_rect(x+7,y+7,l.cw-14,l.ch-14,4,false,true,2)
  if visible>=l.ch then
   local size=l.cw>=72 and l.ch>=96 and 46 or 18;local pad=size==46 and 5 or 4
   local hx,hy=x+(l.cw-size)//2-pad,y+(l.ch-size)//2-pad
   smudge.rounded_rect(hx,hy,size+2*pad,size+2*pad,6,true,false)
   suit(hx+pad,hy+pad,size,2,false)
  end
  return
 end
 local rank=({'A','2','3','4','5','6','7','8','9','10','J','Q','K'})[C.rankOf(c)+1]
 local s=C.suitOf(c);local outline=C.isRed(c)
 F.text(rank,x+9,y+9,20)
 if sideways then suit(x+9,y+48,18,s,outline)
 else suit(x+l.cw-26,y+8,18,s,outline)end
 if visible>=l.ch and l.cw>=72 and l.ch>=90 then suit(x+l.cw//2-20,y+l.ch-57,46,s,outline)end
end
function V.draw(l,C,g,selected,cursor,index,notice,options)
 smudge.clear();l.buttons={}
 smudge.rect(0,0,l.w,56,true,true);smudge.rect(0,60,l.w,3,true,true)
 F.text('SOLITAIRE',16,9,30,true)
 local undo={x=l.w-228,y=8,w=100,h=40};l.buttons[3]=undo
 local canUndo=#g.history>0
 if canUndo then smudge.rect(undo.x,undo.y,undo.w,undo.h,true,false) end
 F.buttonText('UNDO',undo,not canUndo)
 smudge.rect(l.menuButton.x,l.menuButton.y,l.menuButton.w,l.menuButton.h,true,false)
 F.buttonText('NEW',l.menuButton,false)

 for p=0,5 do
  local a=g.piles[p];local x=l.x+l.pileColumns[p]*(l.cw+l.gap)
  if p==1 and #a>0 then
   local shown=math.min(3,#a)
   for i=shown-1,0,-1 do V.card(l,C,x+(shown-1-i)*l.wasteFan,l.top,a[#a-i],false,false,i==0 and l.ch or l.wasteFan,i~=0)end
  elseif #a==0 then F.slot(x,l.top,l.cw,l.ch)
  else V.card(l,C,x,l.top,a[#a])end
 end
 if C.canAutoFinish(g)then
  local b={x=l.x+2*(l.cw+l.gap)+22,y=l.top+(l.ch-56)//2,w=82,h=56};l.buttons[4]=b
  smudge.rect(b.x,b.y,b.w,b.h,true,true);F.buttonText('FINISH',b,true)
 end

 l.pileSteps={};l.pileOffsets={}
 for p=6,12 do
  local a=g.piles[p];local room=l.bottom-l.tab-l.ch
  local down,firstUp,up=14,30,30;local hidden=0
  for i=1,#a-1 do if not C.isFaceUp(a[i])then hidden=hidden+1 end end
  local open=math.max(0,#a-1-hidden);local first=open>0 and 1 or 0;local rest=open-first
  local function height()return hidden*down+first*firstUp+rest*up end
  while height()>room and down>6 do down=down-1 end
  while height()>room and up>16 do up=up-1 end
  if height()>room and hidden+rest>0 then down=math.max(3,(room-first*firstUp)//(hidden+rest));up=down end
  if height()>room and #a>1 then down=math.max(3,room//(#a-1));up=down;firstUp=down end
  local offsets={};l.pileOffsets[p]=offsets;local offset=0;local seenOpen=false
  for i,c in ipairs(a)do
   offsets[i]=offset;local step=down
   if C.isFaceUp(c)then step=seenOpen and up or firstUp;seenOpen=true end
   if i<#a then offset=offset+step end
  end
  l.pileSteps[p]=down
  local x=l.x+(p-6)*(l.cw+l.gap)
  if #a==0 then F.slot(x,l.tab,l.cw,l.ch)end
  for i,c in ipairs(a)do V.card(l,C,x,l.tab+offsets[i],c,false,false,i<#a and offsets[i+1]-offsets[i]or l.ch)end
 end

 if selected then
  local p=selected.p;local a=g.piles[p]
  local x=l.x+(p>=6 and p-6 or l.pileColumns[p])*(l.cw+l.gap)
  if p==1 then x=x+math.max(0,math.min(3,#a)-1)*l.wasteFan end
  local y=p>=6 and l.tab+l.pileOffsets[p][#a-selected.count+1] or l.top
  local height=p>=6 and selected.count>1 and l.pileOffsets[p][#a]-l.pileOffsets[p][#a-selected.count+1]+l.ch or l.ch
  smudge.rounded_rect(x,y,l.cw,height,8,false,true,6)
 end

end
return V
