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
 F.box(b.x,b.y,b.w,b.h,true,true);if dim then smudge.rect_dither(b.x+4,b.y+4,b.w-8,b.h-8,false)end;F.box(b.x,b.y,b.w,b.h,false,false,selected and 3 or 1);mark(good,b,size or 32)
end
local function header(w,title,right)
 smudge.rect(0,0,w,76,true,true);F.text(title,16,19,30,true);if right then F.text(right,w-16-F.width(right,20),26,20,true)end;smudge.rect(0,80,w,3,true,true)
end
function V.clear()V.mask=0;V.give={0,0,0,0,0,0,0};V.camel=false;V.focus=1 end
function V.init(saved,record)
 F.init();V.saved=saved;V.record=record;V.menu=true;V.help=0;V.confirm=false;V.inspect=false;V.clear()
 for i,name in ipairs({'good_diamond','good_gold','good_silver','good_cloth','good_spice','good_leather','camel','bonus_token'})do masks[i]={};for _,n in ipairs({32,44,56})do masks[i][n]=assert(smudge.read_file('art/'..name..'_'..n..'.raw',500))end end
end
local function total(t)local n=0;for _,v in ipairs(t)do n=n+v end;return n end
local function selectedMove(M,g)
 if V.camel then local m={2};return m,M.legal(g,m)and'TAKE EVERY CAMEL'or'THE MARKET HAS NO CAMELS'end
 if V.mask~=0 then local count=0;local slot=0;for i=1,5 do if V.mask&(1<<(i-1))~=0 then count=count+1;slot=i end end
  if total(V.give)>0 or count>1 then local m={3,V.mask,V.give};return m,M.legal(g,m)and('SWAP '..count..' FOR '..count)or'TRADE AT LEAST TWO FOR TWO'end
  local m={1,slot};return m,M.legal(g,m)and('TAKE '..names[g.market[slot]])or'SEVEN CARDS IN HAND, NO MORE'
 end
 for k=1,6 do if V.give[k]>0 then local m={4,k,V.give[k]};return m,M.legal(g,m)and('SELL '..V.give[k]..' '..names[k])or'DIAMOND, GOLD AND SILVER SELL IN PAIRS'end end
 return nil,g.turn==2 and'THEIR TURN'or'TAKE FROM THE MARKET OR SELL FROM YOUR HAND'
end
local function menu(g,w,h)
 header(w,'JAIPUR');F.text(V.record.played..' PLAYED   '..V.record.won..' WON',16,119,10);smudge.rect(16,151,w-32,3,true,true)
 local rows={};if V.saved then rows[#rows+1]={'CONTINUE','resume'}end;rows[#rows+1]={V.confirm and'DISCARD IT?'or'NEW GAME','new'};rows[#rows+1]={'HOW TO PLAY','help'};rows[#rows+1]={'SAVE AND EXIT','exit'}
 local rh=62;local top=h-16-#rows*68
 for i,row in ipairs(rows)do plate(row[1],box(16,top+(i-1)*68,w-32,rh),row[2],nil,i==V.focus)end
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
local function help(w,h)
 local page=V.help;local r=tutorial[page];header(w,'HOW TO PLAY',page..' OF 8');local x,y,bw=16,119,w-32;F.label(r[1],x,y,bw,56,30);local top=y+80;local col=bw//2
 if page==1 then for s=1,2 do F.label(s==1 and'YOU'or'THEM',x+(s-1)*col,top,col,24,10);F.label(s==1 and'74'or'61',x+(s-1)*col,top+30,col,60,30);for i=1,2 do smudge.circle(x+(s-1)*col+col//2-18+(i-1)*36,top+171,13,s==1 and i==1)end end;arrow(x+col//2,top+104,40)
 elseif page==2 then for s=1,2 do local bx=x+(s-1)*(col+12);F.label(s==1 and'TAKE'or'OR SELL',bx,top,col-12,24,10);card(s==1 and 1 or 5,box(bx+(col-84)//2,top+32,72,100));arrow(bx+(col-12)//2,top+144,40);if s==1 then F.label('INTO YOUR HAND',bx,top+196,col-12,24,10)else chip(5,box(bx+(col-72)//2,top+188,60,46),true)end end
 elseif page==3 then for row=1,3 do local yy=top+(row-1)*88;F.label(({'ONE GOOD','EVERY CAMEL','OR TRADE'})[row],x,yy+21,bw//3,22,10);local sx=x+bw//3;if row==1 then card(4,box(sx,yy,46,64))elseif row==2 then for i=1,3 do card(7,box(sx+(i-1)*52,yy,46,64))end else card(1,box(sx,yy,46,64));card(6,box(sx+52,yy,46,64));F.label('<>',sx+102,yy,30,64,20);card(5,box(sx+138,yy,46,64));card(7,box(sx+190,yy,46,64))end end
 elseif page==4 then for i=1,2 do card(1,box(x+(bw-134)//2+(i-1)*72,top,62,84))end;arrow(w//2,top+94,38);F.label('THE DIAMOND PILE, HIGHEST FIRST',x,top+144,bw,22,10);for i,v in ipairs(tokens[1])do chip(v,box(x+(bw-302)//2+(i-1)*62,top+172,54,44),i<=2)end;F.label('THE TOP TWO ARE NOW YOURS',x,top+226,bw,22,10)
 elseif page==5 then for i=1,3 do local b=cell(x,top,bw,120,3,i,16);F.box(b.x,b.y,b.w,b.h,false,false,3);mark(8,box(b.x,b.y+18,b.w,44),44);F.label(({'SELL 3','SELL 4','SELL 5+'})[i],b.x,b.y+76,b.w,24,10)end;F.label('BIGGER SALE, BIGGER TOKEN',x,top+156,bw,24,10)
 elseif page==6 then for s=1,2 do F.label(s==1 and'YOU'or'THEM',x+(s-1)*col,top,col,22,10);local n=s==1 and 4 or 2;for i=1,n do card(7,box(x+(s-1)*col+(col-n*50+4)//2+(i-1)*50,top+30,46,66))end end;arrow(x+col//2,top+116,40);chip(5,box(x+(col-68)//2,top+166,68,46),true)
 elseif page==7 then F.label('THREE PILES RUN OUT',x,top,bw,22,10);for i,k in ipairs({1,4,5})do card(k,box(x+(bw-198)//2+(i-1)*70,top+30,58,80),false,true)end;F.label('OR THE DECK RUNS DRY',x,top+130,bw,22,10);F.box(x+(bw-58)//2,top+160,58,80,false,false,3)
 end
 if page<8 then
  local lines,current={},'';for word in r[2]:gmatch('%S+')do local test=current==''and word or current..' '..word;if F.width(test,20)>bw and current~=''then lines[#lines+1]=current;current=word else current=test end end;lines[#lines+1]=current
  local yy=y+366+math.max(0,(h-64-(y+366)-#lines*31)//2);for _,line in ipairs(lines)do F.center(line,x,yy,bw,31,20);yy=yy+31 end
 else local yy=top;for line in r[2]:gmatch('[^\n]+')do smudge.circle(x+5,yy+13,5,true);yy=F.wrap(line,x+26,yy,bw-26,10,20)+12 end end
 F.label(page==8 and'TAP TO FINISH'or'TAP TO CONTINUE',x,h-64,bw,22,10)
 for i=1,8 do local b=box((w-182)//2+(i-1)*24,h-30,14,14);F.box(b.x,b.y,b.w,b.h,i==page,false,1,7);hit(box(b.x-5,b.y-5,24,24),'page',i)end
 hit(box(x,y,bw,h-y-64),'advance');hit(box(0,0,w,83),'menu')
end
local function inspect(g,w,h)
 header(w,'TOKEN PILES');for k=1,6 do local y=119+(k-1)*78;mark(k,box(16,y,48,64));local values={};for i=g.depth[k]+1,#tokens[k]do values[#values+1]=tokens[k][i]end;F.label(table.concat(values,'  '),76,y,w-92,64,20);smudge.rect(16,y+72,w-32,1,true,true)end;plate('BACK',box(16,h-68,w-32,52),'inspect')
end
local function scores(M,g,w,h)
 header(w,g.phase==2 and'GAME OVER'or'ROUND OVER');local bw=w-32;local markw=100;local col=(bw-markw)//2;local y=119
 F.label('YOU',16+markw,y,col,30,20);F.label('THEM',16+markw+col,y,col,30,20);y=y+38;smudge.rect(16,y,bw,3,true,true);y=y+8
 local rows={{'GOODS',g.points[1],g.points[2]},{'BONUS',g.bpoints[1],g.bpoints[2]},{'CAMELS',g.herd[1]>g.herd[2]and 5 or 0,g.herd[2]>g.herd[1]and 5 or 0},{'TOTAL',M.score(g,1),M.score(g,2)}}
 for _,r in ipairs(rows)do F.label(r[1],16,y,markw,52,14);F.label(r[2],16+markw,y,col,52,20);F.label(r[3],16+markw+col,y,col,52,20);smudge.rect(16,y+51,bw,1,true,true);y=y+52 end
 local winner=M.winner(g);local verdict=g.phase==2 and(g.seals[1]>=2 and'YOU WIN THE GAME'or'THEY WIN THE GAME')or winner==0 and'A DEAD HEAT, NO SEAL'or winner==1 and'YOU TAKE THE SEAL'or'THEY TAKE THE SEAL';F.label(verdict,16,y+16,bw,56,30)
 for s=1,2 do for i=1,2 do smudge.circle(16+markw+(s-1)*col+col//2-18+(i-1)*36,y+100,13,g.seals[s]>=i)end end
 plate(g.phase==1 and'NEXT ROUND'or'NEW GAME',box(16,h-68,bw,52),g.phase==1 and'act'or'new',1,true);hit(box(0,0,w,83),'menu')
end
function V.draw(M,g,notice)
 smudge.clear();V.controls={};V.model=M;V.g=g;local w,h=smudge.get_bounds()
 if V.help>0 then help(w,h)elseif V.menu then menu(g,w,h)elseif V.inspect then inspect(g,w,h)elseif g.phase>0 then scores(M,g,w,h)else
  header(w,'JAIPUR');hit(box(0,0,w,83),'menu');local x,bw=16,w-32;local report=notice~=''and notice or g.turn==1 and'YOUR TURN'or'THEIR TURN';F.text(report,x,119,10)
  local y=151;local gap=math.max(6,(h-68-y-(34+128+96+40+112+44))//5)
  local theirs=g.points[2]+(g.herd[2]>g.herd[1]and 5 or 0);F.label('SEALS '..g.seals[1]..'-'..g.seals[2]..'   YOU '..M.score(g,1)..'   THEM '..theirs..(g.nb[2]>0 and'+'..g.nb[2]..'?'or'')..'   DECK '..(53-g.at),x,y,bw,34,20);smudge.rect(x,y+31,bw,3,true,true);y=y+34+gap
  for i,k in ipairs(g.market)do local b=cell(x,y,bw,128,5,i,8);local selected=k==7 and V.camel or V.mask&(1<<(i-1))~=0;card(k,b,selected,g.turn~=1 or total(g.hands[1])>=7 and V.mask==0 and k~=7,56);if g.turn==1 and k<8 then if #V.controls+1==V.focus then F.box(b.x,b.y,b.w,b.h,false,false,3)end;hit(b,'market',i)end end;y=y+128+gap
  for k=1,6 do local b=cell(x,y,bw,96,6,k,5);F.box(b.x,b.y,b.w,b.h,false);if g.depth[k]==#tokens[k]then smudge.rect_dither(b.x+3,b.y+3,b.w-6,b.h-6,true)end;F.label(tokens[k][g.depth[k]+1]or'-',b.x,b.y+4,b.w,30,20);mark(k,box(b.x,b.y+34,b.w,42));local n=#tokens[k];local p=math.max(2,math.min(6,(b.w-8)//n-2));local px=b.x+(b.w-n*p-(n-1)*2)//2;for i=1,n do smudge.rect(px+(i-1)*(p+2),b.y+b.h-p-7,p,p,i>g.depth[k],1,true)end;hit(b,'inspect')end;y=y+96+gap
  for i=1,3 do local b=cell(x,y,bw*3//5,40,3,i,8);F.box(b.x,b.y,b.w,b.h,false);F.label((i+2)..'+ X'..(bonuses[i]-g.bdepth[i]),b.x,b.y,b.w,b.h,14);hit(b,'inspect')end;y=y+40+gap
  for k=1,6 do local b=cell(x,y,bw,112,6,k,5);F.box(b.x,b.y,b.w,b.h,true,true);if g.hands[1][k]==0 then smudge.rect_dither(b.x+3,b.y+3,b.w-6,b.h-6,false)end;F.box(b.x,b.y,b.w,b.h,false,false,(V.give[k]>0 or #V.controls+1==V.focus)and 3 or 1);F.label((V.give[k]>0 and V.give[k]..'/'or'')..g.hands[1][k],b.x,b.y+6,b.w,38,20);mark(k,box(b.x,b.y+44,b.w,68),44);if g.turn==1 and g.hands[1][k]>0 then hit(b,'hand',k)end end;y=y+112+gap
  local b=cell(x,y,bw,44,2,1,12);F.box(b.x,b.y,b.w,b.h,false,false,V.give[7]>0 and 3 or 1);F.label('YOUR '..(V.give[7]>0 and V.give[7]..'/'or'')..g.herd[1]..' CAMELS',b.x,b.y,b.w,b.h,14);if g.turn==1 and V.mask~=0 then hit(b,'hand',7)end;b=cell(x,y,bw,44,2,2,12);F.box(b.x,b.y,b.w,b.h,false);F.label('THEM '..total(g.hands[2])..' CARDS  '..g.herd[2]..' CAMELS',b.x,b.y,b.w,b.h,14)
  local move,status=selectedMove(M,g);V.move=move;V.canCommit=g.turn==1 and move and M.legal(g,move)
  local clear=V.mask~=0 or V.camel or total(V.give)>0;local width=clear and bw-86 or bw;plate(status,box(x,h-68,width,52),V.canCommit and'commit'or nil,nil,V.canCommit)
  if clear then plate('CLEAR',box(w-94,h-68,78,52),'clear')end
 end
 V.focus=math.max(1,math.min(V.focus,#V.controls))
end
local function route(c)
 if not c then return end;local g,M=V.g,V.model
 if c.op=='act'then return c.value elseif c.op=='commit'then if V.canCommit then return V.move end
 elseif c.op=='resume'then V.menu=false;V.confirm=false
 elseif c.op=='menu'then V.menu=true;V.help=0;V.inspect=false;V.confirm=false
 elseif c.op=='exit'then return'exit'
 elseif c.op=='new'then if not V.saved or V.confirm or g.phase==2 then V.saved=true;V.confirm=false;V.menu=false;return'new'else V.confirm=true end
 elseif c.op=='help'then V.help=1
 elseif c.op=='page'then V.help=c.value
 elseif c.op=='advance'then if V.help==8 then V.help=0;V.menu=true else V.help=V.help+1 end
 elseif c.op=='inspect'then V.inspect=not V.inspect
 elseif c.op=='clear'then V.clear()
 elseif c.op=='market'then local i=c.value;if g.market[i]==7 then V.clear();V.camel=true else V.camel=false;V.mask=V.mask~(1<<(i-1)) end
 elseif c.op=='hand'then local k=c.value;V.camel=false;if V.mask==0 then for j=1,7 do if j~=k then V.give[j]=0 end end end;local max=k==7 and math.min(5,g.herd[1])or g.hands[1][k];V.give[k]=(V.give[k]+1)%(max+1)
 end
 V.focus=1
end
function V.touch(x,y)for i=#V.controls,1,-1 do local c=V.controls[i];local b=c.box;if x>=b.x and y>=b.y and x<b.x+b.w and y<b.y+b.h then V.focus=i;return route(c)end end end
function V.activate()return route(V.controls[V.focus])end
function V.button(b)if #V.controls==0 then return end;local d=(b=='up'or b=='left'or b=='page_back')and -1 or 1;V.focus=(V.focus-1+d)%#V.controls+1 end
function V.playing()return not V.menu and V.help==0 and not V.inspect end
return V
