local identityOk,identity=pcall(smudge.get_device)
if not identityOk or identity~='sticky'then return dofile('legacy_board.lua')end
-- JaipurActivity's market, piles, hand and herd canvas, with its original masks.
local F=dofile('graphics.lua');local V={menu=true,help=0,focus=1,controls={},mask=0,give={0,0,0,0,0,0,0}}
local names={'DIAMOND','GOLD','SILVER','CLOTH','SPICE','LEATHER','CAMEL'}
local tokens={{7,7,5,5,5},{6,6,5,5,5},{5,5,5,5,5},{5,3,3,2,2,1,1},{5,3,3,2,2,1,1},{4,3,2,1,1,1,1,1,1}}
local masks={};local bonuses={7,6,5}
local function box(x,y,w,h)return{x=x,y=y,w=w,h=h}end
local function cell(x,y,w,h,n,i,gap)local cw=(w-(n-1)*gap)//n;return box(x+(i-1)*(cw+gap),y,cw,h)end
local function hit(b,op,value)V.controls[#V.controls+1]={box=b,op=op,value=value}end
local function plate(text,b,op,value,fill)
 F.box(b.x,b.y,b.w,b.h,fill,false,#V.controls+1==V.focus and 3 or 1,b.h==52 and 20 or 8);F.label(text,b.x,b.y,b.w,b.h,20,fill);if op then hit(b,op,value)end
end
local function mark(good,b,size)
 if not masks[good]then return end;size=size or 32;smudge.bitmap(b.x+(b.w-size)//2,b.y+(b.h-size)//2,size,size,masks[good][size])
end
local function card(good,b,selected,dim,size)
 F.box(b.x,b.y,b.w,b.h,true,true);if dim then smudge.rect_dither(b.x+4,b.y+4,b.w-8,b.h-8,false)end;smudge.rect(b.x,b.y,b.w,b.h,false,selected and 4 or 1,true);mark(good,b,size or 32)
end
local function header(w,title,right)
 smudge.rect(0,0,w,76,true,true);F.text(title,16,19,30,true);if right then F.native(right,box(16,27,w-32,42),20,'right',true)end;smudge.rect(0,80,w,3,true,true)
end
function V.clear()V.mask=0;V.give={0,0,0,0,0,0,0};V.camel=false;V.focus=1 end
function V.init(saved,record)
 F.init();V.saved=saved;V.record=record;V.menu=true;V.help=0;V.confirm=false;V.inspect=false;V.showScores=false;V.unavailable=false;V.clear()
 masks.nearby=assert(smudge.read_file('art/nearby32.raw',128));masks.dot=assert(smudge.read_file('art/tutorial-dot.raw',29));V.tutorialPage=nil;V.tutorialPixels=nil
 for i,name in ipairs({'good_diamond','good_gold','good_silver','good_cloth','good_spice','good_leather','camel','bonus_token'})do masks[i]={};for _,n in ipairs({32,44,56})do masks[i][n]=assert(smudge.read_file('art/'..name..'_'..n..'.raw',500))end end
end
local function total(t)local n=0;for _,v in ipairs(t)do n=n+v end;return n end
local function selectedMove(M,g)
 if V.camel then local m={2};local n=0;for _,k in ipairs(g.market)do if k==7 then n=n+1 end end;return m,'TAKE '..n..(n==1 and' CAMEL'or' CAMELS')end
 if V.mask~=0 then local count=0;local slot=0;for i=1,5 do if V.mask&(1<<(i-1))~=0 then count=count+1;slot=i end end
  if total(V.give)>0 or count>1 then local m={3,V.mask,V.give};return m,'EXCHANGE '..total(V.give)..' FOR '..count end
  local m={1,slot};return m,'TAKE '..names[g.market[slot]]
 end
 for k=1,6 do if V.give[k]>0 then local m={4,k,V.give[k]};if k<=3 and V.give[k]<2 then return m,names[k]..' SELLS IN PAIRS'end;local value=0;for i=1,V.give[k]do value=value+(tokens[k][g.depth[k]+i]or 0)end;return m,'SELL '..V.give[k]..' '..names[k]..' -> '..value end end
 return nil,g.turn==2 and'THEY ARE TRADING'or'YOUR MOVE'
end
-- A transient report from an actual successful move; restored saves have no invented last move.
function V.describe(a,before,g)
 local who=before.seat==1 and'YOU'or'THEY';local text
 if a[1]==1 then text=who..' TOOK A '..names[before.card]
 elseif a[1]==2 then local n=g.herd[before.seat]-before.herd;text=who..' TOOK '..n..(n==1 and' CAMEL'or' CAMELS')
 elseif a[1]==3 then local n=0;for i=1,5 do if a[2]&(1<<(i-1))~=0 then n=n+1 end end;text=who..' TRADED '..n..' CARDS'
 else text=who..' SOLD '..a[3]..' '..names[a[2]]..' FOR '..(g.points[before.seat]-before.points);if g.nb[before.seat]>before.bonus then text=text..' + BONUS'end end
 V.report=text
end
local function menu(g,w,h)
 header(w,'JAIPUR');F.native(V.record.played..' PLAYED   '..V.record.won..' WON',box(16,119,w-32,26),10,'left');smudge.rect(16,151,w-32,3,true,true)
 local rows={};if V.saved then rows[#rows+1]={'CONTINUE','resume','ROUND '..g.round}end
 rows[#rows+1]={V.confirm and'DISCARD IT?'or'NEW GAME','new'};rows[#rows+1]={'PLAY NEARBY','nearby'};rows[#rows+1]={'HOW TO PLAY','help'}
 local count=#rows;local top=h-16-(count*62+(count-1)*6+12)
 local slotY,slotH=145,top-145;local rh=math.min(52,(slotH-24)//6);local py=slotY+(slotH-24-6*rh)//2
 F.capsLabel('WHAT EACH GOOD PAYS, BEST FIRST',box(16,py,w-32,24),10,'left');smudge.rect(16,py+24,w-32,3,true,true)
 local cellW=(w-32-44)//9
 for good=1,6 do local y=py+24+(good-1)*rh
  smudge.bitmap(16+(44-32)//2,y+(rh-32)//2,32,32,masks[good][32])
  for i,value in ipairs(tokens[good])do F.capsLabel(value,box(16+44+(i-1)*cellW,y,cellW,rh),20)end
  if good<6 then smudge.rect_dither(16,y+rh-1,w-32,1,true)end
 end
 for i,row in ipairs(rows)do
  local b=box(16,top+(i-1)*66,w-32,62);local chosen=i==V.focus
  smudge.rect(b.x,b.y,b.w,b.h,chosen,1,true)
  F.native(row[1],box(b.x+12,b.y,b.w-24,b.h),20,'left',chosen)
  if row[3]then F.native(row[3],box(b.x+12,b.y,b.w-24,b.h),20,'right',chosen)end
  if row[2]=='nearby'then smudge.bitmap(b.x+b.w-56,b.y+15,32,32,masks.nearby,chosen)end
  hit(b,row[2])
 end
end
local tutorial={
 {'THE GOAL','THE RICHER TRADER TAKES A SEAL OF EXCELLENCE. TWO SEALS WINS THE GAME, SO A MATCH IS TWO OR THREE ROUNDS.'},
 {'YOUR TURN','EVERY TURN IS ONE OR THE OTHER. TAKE CARDS FROM THE MARKET, OR SELL FROM YOUR HAND.'},
 {'TAKING','TAKE ONE GOOD, OR EVERY CAMEL AT ONCE, OR TRADE AT LEAST TWO FOR TWO. CAMELS COUNT AS PAYMENT.'},
 {'SELLING','SELL ANY NUMBER OF ONE GOOD. ITS TOKENS COME OFF THE TOP, SO SELLING EARLY PAYS MORE.'},
 {'THE BONUS','SELL THREE OR MORE OF ONE GOOD AT ONCE AND TAKE A BONUS TOKEN. IT STAYS FACE DOWN UNTIL THE ROUND IS SCORED.'},
 {'CAMELS','CAMELS NEVER SELL AND NEVER COUNT AGAINST YOUR HAND. THE BIGGER HERD TAKES FIVE RUPEES WHEN THE ROUND ENDS.'},
 {'THE END','EITHER ONE ENDS THE ROUND ON THE SPOT. COUNT UP, TAKE THE SEAL, AND DEAL AGAIN, WITH THE LOSER GOING FIRST.'},
 {'THE SMALL PRINT','SEVEN CARDS IN HAND, NO MORE. CAMELS SIT IN YOUR HERD AND NEVER COUNT.\nDIAMOND, GOLD AND SILVER GO TWO OR MORE AT A TIME. NEVER ONE ON ITS OWN.\nA TRADE CANNOT TAKE AND GIVE THE SAME GOOD.\nCAMELS CAN BE GIVEN IN A TRADE. THEY CAN NEVER BE TAKEN IN ONE.\nTAKING CAMELS TAKES EVERY CAMEL IN THE MARKET.\nA PILE WITH TOO FEW TOKENS PAYS WHAT IS LEFT, AND STILL PAYS THE BONUS.\nEQUAL HERDS: NOBODY TAKES THE FIVE RUPEES.\nTAKING REFILLS THE MARKET FROM THE DECK. TRADING DOES NOT.'}}
local function arrow(x,y,len)
 smudge.rect(x-1,y,3,len-8,true,true);smudge.line(x-8,y+len-10,x,y+len);smudge.line(x,y+len,x+8,y+len-10)
end
local function chip(value,b,taken)F.box(b.x,b.y,b.w,b.h,taken);F.label(value,b.x,b.y,b.w,b.h,20,taken)end
local function paragraph(text,b,size,draw)
 local lines,line={},'';for word in text:gmatch('%S+')do local next=line==''and word or line..' '..word;if line~=''and F.nativeWidth(next,size)>b.w then lines[#lines+1]=line;line=word else line=next end end;if line~=''then lines[#lines+1]=line end
 local lh=size==20 and 42 or 21;if draw then for i,run in ipairs(lines)do F.native(run,box(b.x,b.y+(i-1)*lh,b.w,lh),size,draw)end end;return #lines*lh
end
local function help(w,h)
 local page=V.help;local r=tutorial[page];header(w,'HOW TO PLAY',page..' OF 8');local x,y,bw=16,119,w-32
 F.native(r[1],box(x,y,bw,46),30,'center')
 if page<8 then
  -- Original immutable tutorial illustrations, generated by actual native Screens.
  if V.tutorialPage~=page then V.tutorialPixels=assert(smudge.read_file('art/tutorial-'..page..'.raw',15681));V.tutorialPage=page end;smudge.bitmap(x,199,448,280,V.tutorialPixels)
  local size=20;local used=paragraph(r[2],box(x,485,bw,243),size);if used>243 then size=10;used=paragraph(r[2],box(x,485,bw,243),size)end
  paragraph(r[2],box(x,485+math.max(0,(243-used)//2),bw,used),size,'center')
 else local yy=199;for line in r[2]:gmatch('[^\n]+')do smudge.rounded_rect(x,yy+8,10,10,5,true,true,1);yy=yy+paragraph(line,box(x+26,yy,bw-26,200),10,'left')+12 end end
 F.native(page==8 and'TAP TO FINISH'or'TAP TO CONTINUE',box(x,h-64,bw,22),10,'center')
 local dot=masks.dot
 for i=1,8 do local b=box((w-182)//2+(i-1)*24,h-30,14,14);if i==page then smudge.rounded_rect(b.x,b.y,14,14,7,true,true,1)else smudge.bitmap(b.x,b.y,14,14,dot)end;hit(box(b.x-5,b.y-5,24,24),'page',i)end
 hit(box(x,y,bw,h-y-64),'advance');hit(box(0,0,w,83),'menu')
end
local function inspect(g,w,h)
 header(w,'TOKEN PILES');for k=1,6 do local y=119+(k-1)*78;mark(k,box(16,y,48,64));local values={};for i=g.depth[k]+1,#tokens[k]do values[#values+1]=tokens[k][i]end;F.label(table.concat(values,'  '),76,y,w-92,64,20);smudge.rect(16,y+72,w-32,1,true,true)end;plate('BACK',box(16,h-68,w-32,52),'inspect')
end
local function scores(M,g,w,h)
 header(w,g.phase==2 and'JAIPUR'or'ROUND '..g.round);local x,bw=16,w-32;local markw=76;local col=(bw-markw)//2
 F.capsLabel('YOU',box(x+markw,119,col,30),20);F.capsLabel('THEM',box(x+markw+col,119,col,30),20);smudge.rect(x,153,bw,3,true,true)
 -- The existing saved model keeps combined goods points, not six per-seat payouts.
 -- Keep the original eight-row ledger geometry without inventing six numbers.
 for good=1,6 do mark(good,box(x,160+(good-1)*44,markw,44),32)end
 for seat=1,2 do F.capsLabel(g.points[seat],box(x+markw+(seat-1)*col,160,col,220),20);F.capsLabel('COMBINED GOODS',box(x+markw+(seat-1)*col,380,col,44),10)end
 smudge.rect(x,423,bw,1,true,true)
 for i,k in ipairs({8,7})do local y=424+(i-1)*44;mark(k,box(x,y,markw,44),32)
  for seat=1,2 do local v=k==8 and g.bpoints[seat]or(g.herd[seat]>g.herd[3-seat]and 5 or 0);F.capsLabel(v,box(x+markw+(seat-1)*col,y,col,44),20)end
  if i==1 then smudge.rect(x,y+43,bw,1,true,true)end
 end
 smudge.rect(x,516,bw,3,true,true);F.capsLabel('RUPEES',box(x,529,markw,60),10,'left')
 for seat=1,2 do F.capsLabel(M.score(g,seat),box(x+markw+(seat-1)*col,529,col,60),30)end
 local winner=M.winner(g);local verdict=g.phase==2 and(g.seals[1]>=2 and'YOU WIN THE GAME'or'THEY WIN THE GAME')or winner==0 and'A DEAD HEAT, NO SEAL'or winner==1 and'YOU TAKE THE SEAL'or'THEY TAKE THE SEAL'
 F.box(x,599,bw,56,true,false,1,20);F.capsLabel(verdict,box(x+12,599,bw-24,56),20,'center',true)
 for seat=1,2 do local cx=x+markw+(seat-1)*col+col//2
  for i=1,2 do local px=cx-26+(i-1)*30;if g.seals[seat]>=i then smudge.rounded_rect(px,669,22,22,11,true,true,1)else smudge.rect(px,669,22,22,false,3,true)end end
 end
 local foot=box(x,h-68,bw,52);F.button(g.phase==1 and'NEXT ROUND'or'PLAY AGAIN',foot,true,false,true);hit(foot,g.phase==1 and'act'or'new',1);hit(box(0,0,w,83),'menu')
end
function V.draw(M,g,notice)
 smudge.clear();V.controls={};V.model=M;V.g=g;local w,h=smudge.get_bounds()
 if V.help>0 then help(w,h)elseif V.menu then menu(g,w,h)elseif V.inspect then inspect(g,w,h)elseif V.unavailable then header(w,'PLAY NEARBY');F.native('NEARBY PLAY IS UNAVAILABLE ON THIS DEVICE.',box(16,220,w-32,150),20,'center',false,4);F.native('YOUR LOCAL GAME AND SAVES ARE KEPT.',box(16,400,w-32,110),20,'center',false,3);F.button('BACK',box(16,h-68,w-32,52),true,false,true);hit(box(16,h-68,w-32,52),'closeNearby')
 elseif g.phase>0 and V.showScores then scores(M,g,w,h)else
  header(w,'JAIPUR');hit(box(0,0,w,83),'menu');local x,bw=16,w-32
  F.native(notice~=''and notice or V.report or'',box(x,119,bw,26),10,'left')
  local y=151;local gap=math.max(6,(h-68-y-(34+128+96+40+112+44))//5)
  local theirs=g.points[2]+(g.herd[2]>g.herd[1]and 5 or 0)
  F.capsLabel('SEALS '..g.seals[1]..'-'..g.seals[2]..'   YOU '..M.score(g,1)..'   THEM '..theirs..(g.nb[2]>0 and'+'..g.nb[2]..'?'or'')..'   DECK '..(53-g.at),box(x,y,bw,34),20,'left');smudge.rect(x,y+31,bw,3,true,true);y=y+34+gap
  for i,k in ipairs(g.market)do local b=cell(x,y,bw,128,5,i,8);local selected=k==7 and V.camel or V.mask&(1<<(i-1))~=0
   if k==8 then smudge.rect(b.x,b.y,b.w,b.h,false,1,true)else card(k,b,selected,g.phase~=0 or g.turn~=1 or total(g.hands[1])>=7 and V.mask==0 and not V.camel and k~=7,56)end
   if g.phase==0 and g.turn==1 and k<8 then hit(b,'market',i)end
  end;y=y+128+gap
  for k=1,6 do local b=cell(x,y,bw,96,6,k,5)
   F.box(b.x,b.y,b.w,b.h,true,true,1,6);if g.depth[k]==#tokens[k]then smudge.rect_dither(b.x+3,b.y+3,b.w-6,b.h-6,true)end
   smudge.rect(b.x,b.y,b.w,b.h,false,1,true)
   F.capsLabel(tokens[k][g.depth[k]+1]or'-',box(b.x+3,b.y+4,b.w-6,30),20)
   local n=#tokens[k];local pip=math.max(2,math.min(6,(b.w-8)//n-2));local py=b.y+b.h-pip-7
   smudge.bitmap(b.x+(b.w-32)//2,b.y+34+(py-b.y-34-32)//2,32,32,masks[k][32])
   local px=b.x+(b.w-n*pip-(n-1)*2)//2
   for i=1,n do smudge.rect(px+(i-1)*(pip+2),py,pip,pip,i>g.depth[k],1,true)end;hit(b,'inspect')
  end;y=y+96+gap
  for i=1,3 do local b=cell(x,y,bw*3//5,40,3,i,8);F.box(b.x,b.y,b.w,b.h,true,true,1,6);smudge.rect(b.x,b.y,b.w,b.h,false,1,true);F.capsLabel((i+2)..'+ x'..(bonuses[i]-g.bdepth[i]),box(b.x+4,b.y,b.w-8,b.h),20);hit(b,'inspect')end;y=y+40+gap
  for k=1,6 do local b=cell(x,y,bw,112,6,k,5);F.box(b.x,b.y,b.w,b.h,true,true)
   if g.hands[1][k]==0 then smudge.rect_dither(b.x+3,b.y+3,b.w-6,b.h-6,false)end
   smudge.rect(b.x,b.y,b.w,b.h,false,V.give[k]>0 and 4 or 1,true)
   F.capsLabel((V.give[k]>0 and V.give[k]..'/'or'')..g.hands[1][k],box(b.x+4,b.y+6,b.w-8,38),20)
   smudge.bitmap(b.x+(b.w-44)//2,b.y+44+(b.h-44-44)//2,44,44,masks[k][44])
   if g.phase==0 and g.turn==1 and g.hands[1][k]>0 then hit(b,'hand',k)end
  end;y=y+112+gap
  local b=cell(x,y,bw,44,2,1,12);F.box(b.x,b.y,b.w,b.h,true,true,1,6)
  if g.turn~=1 or V.mask==0 then smudge.rect_dither(b.x+3,b.y+3,b.w-6,b.h-6,false)end
  smudge.rect(b.x,b.y,b.w,b.h,false,V.give[7]>0 and 4 or 1,true)
  F.capsLabel(V.give[7]>0 and('YOUR CAMELS '..V.give[7]..'/'..g.herd[1])or('YOUR '..g.herd[1]..(g.herd[1]==1 and' CAMEL'or' CAMELS')),box(b.x+6,b.y,b.w-12,b.h),20)
  if g.phase==0 and g.turn==1 and V.mask~=0 then hit(b,'hand',7)end
  b=cell(x,y,bw,44,2,2,12);smudge.rect(b.x,b.y,b.w,b.h,false,1,true)
  F.capsLabel('THEM '..total(g.hands[2])..' CARDS  '..g.herd[2]..(g.herd[2]==1 and' CAMEL'or' CAMELS'),box(b.x+6,b.y,b.w-12,b.h),20)
  local move,status=selectedMove(M,g);if g.phase>0 then status='SEE SCORES'end;V.move=move;V.canCommit=g.phase>0 or g.turn==1 and move and M.legal(g,move)
  local footer=box(x,h-68,bw,52)
  if V.canCommit then smudge.rect(footer.x,footer.y,footer.w,footer.h,true,true)else smudge.rect_dither(footer.x,footer.y,footer.w,footer.h,true)end
  F.button(status,footer,nil,false,true);if V.canCommit then hit(footer,'commit')end

 end
 V.focus=math.max(1,math.min(V.focus,#V.controls))
end
local function route(c)
 if not c then return end;local g,M=V.g,V.model
 if c.op=='act'then V.showScores=false;return c.value elseif c.op=='commit'then if g.phase>0 then V.showScores=true elseif V.canCommit then return V.move end
 elseif c.op=='resume'then V.menu=false;V.confirm=false
 elseif c.op=='menu'then V.menu=true;V.help=0;V.inspect=false;V.confirm=false
 elseif c.op=='nearby'then V.menu=false;V.unavailable=true
 elseif c.op=='closeNearby'then V.unavailable=false;V.menu=true
 elseif c.op=='exit'then return'exit'
 elseif c.op=='new'then
  -- Retire the old NEW GAME contact before DISCARD IT? is painted.
  V.controls={}
  if not V.saved or V.confirm or g.phase==2 then V.saved=true;V.confirm=false;V.menu=false;V.showScores=false;return'new'else V.confirm=true end
 elseif c.op=='help'then V.help=1
 elseif c.op=='page'then V.help=c.value
 elseif c.op=='advance'then if V.help==8 then V.help=0;V.menu=true else V.help=V.help+1 end
 elseif c.op=='inspect'then V.inspect=not V.inspect
 elseif c.op=='clear'then V.clear()
 elseif c.op=='market'then local i=c.value;if g.market[i]==7 then local was=V.camel;V.clear();V.camel=not was else if V.camel then V.clear()end;V.camel=false;V.mask=V.mask~(1<<(i-1));V.give={0,0,0,0,0,0,0} end
 elseif c.op=='hand'then local k=c.value;V.camel=false;if V.mask==0 then for j=1,7 do if j~=k then V.give[j]=0 end end end;local max=k==7 and g.herd[1]or g.hands[1][k];if V.mask~=0 then local wanted=0;for i=1,5 do if V.mask&(1<<(i-1))~=0 then wanted=wanted+1 end end;max=math.min(max,wanted)end;V.give[k]=(V.give[k]+1)%(max+1)
 end
 V.focus=1
end
function V.touch(x,y)for i=#V.controls,1,-1 do local c=V.controls[i];local b=c.box;if x>=b.x and y>=b.y and x<b.x+b.w and y<b.y+b.h then V.focus=i;return route(c)end end;if V.playing()then V.clear()end end
function V.activate()return route(V.controls[V.focus])end
function V.button(b)if #V.controls==0 then return end;local d=(b=='up'or b=='left'or b=='page_back')and -1 or 1;V.focus=(V.focus-1+d)%#V.controls+1 end
function V.playing()return not V.menu and V.help==0 and not V.inspect and not V.unavailable end
return V
