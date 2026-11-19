-- SeaSaltScreens.cpp card, pile, tab and hint geometry, using its original masks.
local F=dofile('graphics.lua');local cards=dofile('cards.lua');local kinds,colours=cards[1],cards[2]
local V={menu=true,help=0,tab=0,page=1,focus=1};local icons,marks={},{};local controls={};local context
local names={'CRAB','BOAT','FISH','SWIMMER','SHARK','SHELL','TURTLE','GULL','SAILOR','MERMAID','LIGHTHOUSE','SHOAL','NEST','CAPTAIN'}
local artnames={'crab','boat','fish','swimmer','shark','shell','octopus','penguin','sailor','mermaid','lighthouse','shoal','nest','captain'}
local marknames={'circle','circle_o','square','square_o','triangle','triangle_o','diamond','diamond_o','plus','x','asterisk'}
local supply={};for _,kind in ipairs(kinds)do supply[kind+1]=(supply[kind+1]or 0)+1 end
local hints={
 'PAIRS WITH ANOTHER CRAB. THE PAIR DIGS ANY CARD OUT OF A PILE.',
 'PAIRS WITH ANOTHER BOAT. THE PAIR BUYS ANOTHER TURN.',
 'PAIRS WITH ANOTHER FISH. THE PAIR DRAWS A CARD.',
 'PAIRS WITH A SHARK. THE PAIR STEALS FROM THEIR HAND.',
 'PAIRS WITH A SWIMMER. THE PAIR STEALS FROM THEIR HAND.',
 'WORTH 0-2-4-6-8-10 AS YOU COLLECT 1 TO 6.',
 'WORTH 0-3-6-9-12 AS YOU COLLECT 1 TO 5.',
 'WORTH 1-3-5 AS YOU COLLECT 1 TO 3.',
 'TWO SAILORS ARE 5. THE CAPTAIN PAYS 3 EACH.',
 'SCORES YOUR BIGGEST COLOUR. HOLD ALL FOUR AND YOU WIN.',
 '1 POINT PER BOAT YOU HOLD.','1 POINT PER FISH YOU HOLD.',
 '2 POINTS PER GULL YOU HOLD.','3 POINTS PER SAILOR YOU HOLD.'}
local function rect(b,fill,paper,stroke)
 if fill then smudge.rect(b.x,b.y,b.w,b.h,true,not paper)else smudge.rect(b.x,b.y,b.w,b.h,false,stroke or 1,not paper)end
end
local function hit(b,op,value)controls[#controls+1]={box=b,op=op,value=value};return #controls end
local function fits(s,width,size)if F.width(s,size)>width then size=10 end;return F.fit(s,width,size),size end
local function label(text,b,size,paper)local s,n=fits(text,b.w-8,size or 20);F.center(s,b.x,b.y,b.w,b.h,n,paper)end
local function button(text,b,op,value,filled)
 local n=#controls+1;local active=filled or n==V.focus and b.h==62
 if b.h==52 then smudge.rounded_rect(b.x,b.y,b.w,b.h,20,active,true,n==V.focus and 3 or 1)else rect(b,active,false,n==V.focus and 3 or 1)end
 label(text,b,20,active);hit(b,op,value)
end
local function header(w,title,right)
 smudge.rect(0,0,w,76,true,true);F.text(title,16,19,30,true)
 if right then F.text(right,w-16-F.width(right,20),26,20,true)end
 smudge.rect(0,80,w,3,true,true)
end
local function option(options,needle)for i,s in ipairs(options)do if s==needle then return i end end end
local function cardOption(options,c)for i,s in ipairs(options)do if tonumber(s:match('#(%d+)$'))==c then return i end end end
function V.init(saved,record)
 F.init();V.saved=saved;V.record=record or{played=0,won=0}
 for k,name in ipairs(artnames)do icons[k]={};for _,size in ipairs({24,40,48})do icons[k][size]=assert(smudge.read_file('art/ss_'..name..'_'..size..'.raw',400))end end
 for i,name in ipairs(marknames)do marks[i]={};for _,size in ipairs({20,24})do marks[i][size]=assert(smudge.read_file('art/ss_m_'..name..'_'..size..'.raw',72))end end
end
local function picture(kind,x,y,size)smudge.bitmap(x,y,size,size,icons[kind+1][size])end
local function colour(mark,x,y,size)size=size or 20;smudge.bitmap(x,y,size,size,marks[mark+1][size])end
local function groupPoints(M,g,kind,seat)
 local n={};for k=0,13 do n[k+1]=M.count(g,seat,k,false)end
 if kind<=2 then return n[kind+1]//2 elseif kind<=4 then return math.min(n[4],n[5])
 elseif kind==5 then return ({0,0,2,4,6,8,10})[n[6]+1]or 10
 elseif kind==6 then return ({0,0,3,6,9,12})[n[7]+1]or 12
 elseif kind==7 then return ({0,1,3,5})[n[8]+1]or 5
 elseif kind==8 then return n[9]>=2 and 5 or 0
 elseif kind==9 then local counts={};for i=1,11 do counts[i]=0 end;for c=1,58 do if g.place[c]==seat or g.place[c]==seat+2 then counts[colours[c]+1]=counts[colours[c]+1]+1 end end;table.sort(counts,function(a,b)return a>b end);local sum=0;for i=1,n[10]do sum=sum+counts[i]end;return sum
 elseif kind==10 then return n[2]elseif kind==11 then return n[3]elseif kind==12 then return 2*n[8]else return 3*n[9]end
end
local function card(M,g,c,b,action,example)
 local kind=example and example.kind or kinds[c];rect(b,true,true);rect(b,false)
 if not example and(V.selected==c or V.second==c or #controls+1==V.focus)then smudge.rect(b.x-3,b.y-3,b.w+6,b.h+6,false,2,true)end
 local n=math.max(10,math.min(20,b.h//4));colour(example and example.colour or colours[c],b.x+6,b.y+6)
 local points=tostring(example and example.points or groupPoints(M,g,kind,(g.place[c]==2 or g.place[c]==4)and 2 or 1));F.text(points,b.x+b.w-8-F.width(points,20),b.y+5,20)
 local census=b.y+b.h-21;local nameY=census-19;local named=nameY-(b.y+28)>=40
 local faceTop=named and b.y+28 or b.y+20;local room=(named and nameY or census)-faceTop
 local size=room>=48 and 48 or room>=40 and 40 or 24
 picture(kind,b.x+(b.w-size)//2,faceTop+math.max(0,(room-size)//2),size)
 if named then F.center(names[kind+1],b.x,nameY,b.w,15,10)end
 F.center('X'..supply[kind+1],b.x,census,b.w,15,10)
 if not example then hit(b,action and 'act' or 'select',action or c)end
end
local function pile(M,g,p,b,action)
 rect(b,false);local c=M.top(g,p);local count=0;for i=1,58 do if g.place[i]==p+4 then count=count+1 end end
 if c==0 then label('EMPTY',b,10)else
  colour(colours[c],b.x+6,b.y+6);F.text(tostring(count),b.x+b.w-8-F.width(tostring(count),20),b.y+5,20)
  local size=b.h>=85 and 40 or 24;picture(kinds[c],b.x+(b.w-size)//2,b.y+26,size)
  F.center(names[kinds[c]+1]..' X'..supply[kinds[c]+1],b.x,b.y+b.h-23,b.w,16,10)
 end
 if action then hit(b,'act',action)end
end
local function wrap(text,width)
 local lines,current={},''
 for word in text:gmatch('%S+')do
  if current~=''and F.width(current..' '..word,10)>width then lines[#lines+1]=current;current=word else current=current==''and word or current..' '..word end
 end;if current~=''then lines[#lines+1]=current end;return lines
end
local function hint(b,text)
 for x=0,b.w-1,10 do smudge.rect(b.x+x,b.y,math.min(6,b.w-x),2,true,true);smudge.rect(b.x+x,b.y+b.h-2,math.min(6,b.w-x),2,true,true)end
 for y=0,b.h-1,10 do smudge.rect(b.x,b.y+y,2,math.min(6,b.h-y),true,true);smudge.rect(b.x+b.w-2,b.y+y,2,math.min(6,b.h-y),true,true)end
 local lines=wrap(text,b.w-16);for i=1,math.min(#lines,(b.h-8)//18)do F.center(lines[i],b.x+4,b.y+4+(i-1)*18,b.w-8,18,10)end
end
local function drawMenu(M,g,options,w,h)
 header(w,'SEA SALT');F.text(V.record.played..' PLAYED   '..V.record.won..' WON',16,130,10);smudge.rect(16,157,w-32,3,true,true)
 local y=h-16-(V.saved and 4 or 3)*68
 if V.saved then button('CONTINUE',{x=16,y=y,w=w-32,h=62},'continue');y=y+68 end
 button('NEW GAME',{x=16,y=y,w=w-32,h=62},'new');y=y+68
 button('HOW TO PLAY',{x=16,y=y,w=w-32,h=62},'help');y=y+68
 button('EXIT',{x=16,y=y,w=w-32,h=62},'act',option(options,'Exit')or #options)
 local side=math.min(100,(h-340)//2);if side>=48 then picture(9,(w-48)//2,190,48);F.center(g.score[1]..' - '..g.score[2],16,260,w-32,48,30)end
end
local lessons={
 {title='TAKE ONE CARD A TURN.',lines={{84,'DRAW TWO FROM THE DECK AND KEEP ONE,'},{110,'OR TAKE THE TOP OF A DISCARD PILE.'}},cards={{1,0,0},{2,2,0}},cardsY=176,caption={440,'FIRST TO 40 POINTS WINS.'}},
 {title='PAIRS HAVE POWERS.',cards={{1,0,1},{2,2,1},{0,4,1}},cardsY=74,lines={{254,'TWO BOATS: TAKE ANOTHER TURN.'},{280,'TWO FISH: DRAW A CARD.'},{306,'TWO CRABS: DIG THROUGH A PILE.'},{396,'PLAY A PAIR FACE UP TO FIRE ITS POWER.'},{422,'ITS POINT COUNTS PLAYED OR NOT.'}}},
 {title='SWIMMER AND SHARK.',cards={{3,1,0},{4,3,0}},cardsY=74,lines={{254,'PLAY BOTH TOGETHER TO STEAL'},{280,'A RANDOM CARD FROM THEIR HAND.'},{370,'LIKE EVERY DUO, THE PAIR IS A POINT'},{396,'WHETHER PLAYED OR KEPT IN HAND.'}}},
 {title='COLLECTORS GROW.',cards={{5,0,2},{6,1,3},{7,6,3},{8,9,5}},cardsY=74,lines={{254,'SHELLS PAY 0-2-4-6-8-10 AS THEY ADD UP.'},{280,'TURTLES 0-3-6-9-12. GULLS 1-3-5.'},{306,'TWO SAILORS PAY 5.'},{396,'THE ONE-OFF CARDS MULTIPLY THEM:'},{422,'LIGHTHOUSE 1 PER BOAT. SHOAL 1 PER FISH.'},{448,'NEST 2 PER GULL. CAPTAIN 3 PER SAILOR.'}}},
 {title='MERMAIDS COUNT COLOURS.',cards={{9,5,0}},cardsY=74,lines={{254,'EACH MERMAID SCORES YOUR BIGGEST'},{280,'COLOUR GROUP, EACH A DIFFERENT ONE.'},{370,'HOLD ALL FOUR AND YOU WIN ON THE SPOT.'}}},
 {title='ENDING A ROUND.',lines={{84,'FROM 7 POINTS, ON YOUR TURN, CALL IT.'},{164,'STOP: EVERYBODY BANKS THEIR CARDS.'},{244,'LAST CHANCE: A BET THAT YOU HOLD THE MOST.'},{270,'THEY GET ONE MORE TURN. IF YOU STILL LEAD,'},{296,'YOU BANK CARDS PLUS YOUR COLOUR BONUS'},{322,'AND THEY BANK ONLY THEIR BONUS.'},{348,'IF THEY PASS YOU, IT IS REVERSED.'},{430,'IF THE DECK RUNS OUT, NOBODY SCORES.'}}}}
local function tutorial(M,g,w,h)
 header(w,'HOW TO PLAY',V.help..' OF 6');local page=lessons[V.help];local landscape=w>h
 local top=landscape and 95 or 119;local scale=landscape and(h-135)/665 or 1
 local function at(offset)return top+math.floor(offset*scale)end
 F.center(page.title,16,at(16),w-32,26,20)
 for _,line in ipairs(page.lines)do F.center(line[2],16,at(line[1]),w-32,22,10)end
 if page.caption then F.center(page.caption[2],16,at(page.caption[1]),w-32,26,20)end
 if page.cards then local count=#page.cards;local gap=count>=4 and 8 or 16;local x=(w-(count*106+(count-1)*gap))//2
  for _,spec in ipairs(page.cards)do local c
   for id=1,58 do if kinds[id]==spec[1]and colours[id]==spec[2]then c=id;break end end
   if not c then for id=1,58 do if kinds[id]==spec[1]then c=id;break end end end
   assert(c,'Missing original tutorial kind');card(M,g,c,{x=x,y=at(page.cardsY),w=106,h=math.max(75,math.floor(125*scale))},nil,{points=spec[3],kind=spec[1],colour=spec[2]});x=x+106+gap
  end
 end
 local x=(w-96)//2
 for i=1,6 do smudge.rect(x+(i-1)*16,h-34,8,8,i==V.help,1,true)end
 hit({x=16,y=top,w=w-32,h=h-top-16},'lesson')
end
local function round(M,g,options,w,h)
 header(w,g.phase==3 and 'SEA SALT'or'ROUND '..g.round)
 local a,b=M.points(g,1),M.points(g,2);local headline
 if M.count(g,1,9,true)==4 then headline='FOUR MERMAIDS. YOU WIN.'elseif M.count(g,2,9,true)==4 then headline='FOUR MERMAIDS. THEY WIN.'elseif g.ender==0 then headline='DECK OUT. NOBODY SCORES.'else headline=(g.ender==1 and 'YOU'or'THEY')..(g.bet==1 and ' CALLED LAST CHANCE.'or' CALLED STOP.')end
 label(headline,{x=16,y=130,w=w-32,h=44},20)
 F.text('YOUR CARDS '..a..'   THEIR CARDS '..b,16,190,10)
 F.center(g.score[1]..' - '..g.score[2],16,240,w-32,70,30)
 button(g.phase==3 and 'PLAY AGAIN'or'NEXT ROUND',{x=16,y=h-68,w=w-32,h=52},'act',g.phase==2 and 1 or 3)
 button('INSPECT CARDS',{x=16,y=h-132,w=w-32,h=52},'act',2)
end
local function drawBoard(M,g,options,w,h,notice)
 header(w,'SEA SALT',g.score[1]..' - '..g.score[2]);local landscape=w>h
 local facts={x=16,y=119,w=w-32,h=44};local piles={x=16,y=169,w=w-32,h=96};local tabs={x=16,y=271,w=w-32,h=40}
 local primary={x=16,y=h-68,w=w-32-157,h=52};local call={x=w-166,y=h-68,w=150,h=52};local hb={x=16,y=h-130,w=w-32,h=54};local grid={x=16,y=317,w=w-32,h=h-453}
 if landscape then local right=w*3//5+16;local wide=w-right-16
  facts={x=right,y=95,w=wide,h=60};piles={x=right,y=161,w=wide,h=78};tabs={x=16,y=95,w=right-32,h=32};grid={x=16,y=139,w=right-32,h=h-155};hb={x=right,y=251,w=wide,h=h-335};primary={x=right,y=h-68,w=(wide-7)//2,h=52};call={x=right+(wide+7)//2,y=h-68,w=(wide-7)//2,h=52}
 end
 local held,theirs,yours,best,bestColour=0,0,0,0,0;local coloursHeld={};for i=0,10 do coloursHeld[i]=0 end
 for c=1,58 do if g.place[c]==2 then held=held+1 elseif g.place[c]==4 then theirs=theirs+1 elseif g.place[c]==3 then yours=yours+1 end
  if g.place[c]==1 or g.place[c]==3 then local co=colours[c];coloursHeld[co]=coloursHeld[co]+1;if coloursHeld[co]>best then best,bestColour=coloursHeld[co],co end end
 end
 rect(facts,false);local fw=facts.w//4
 for i,pair in ipairs({{held,'THEY','HOLD'},{theirs,'THEIR','TABLE'},{yours,'YOUR','TABLE'}})do local x=facts.x+(i-1)*fw
  if landscape then F.center(tostring(pair[1]),x,facts.y+3,fw,26,20);F.center(pair[2],x,facts.y+30,fw,13,10);F.center(pair[3],x,facts.y+44,fw,13,10)
  else F.text(tostring(pair[1]),x+5,facts.y+10,20);F.center(pair[2],x+25,facts.y+4,fw-27,16,10);F.center(pair[3],x+25,facts.y+22,fw-27,16,10)end
 end
 for i=1,3 do smudge.rect(facts.x+i*fw,facts.y,1,facts.h,true,true)end
 colour(bestColour,facts.x+3*fw+5,facts.y+(facts.h-24)//2,24);F.text(tostring(best),facts.x+3*fw+34,facts.y+(facts.h-25)//2,20)
 local pw=(piles.w-14)//3;local db={x=piles.x,y=piles.y,w=pw,h=piles.h};smudge.rect_dither(db.x,db.y,db.w,db.h,false);rect(db,false);F.center('DECK',db.x,db.y+5,db.w,25,10);F.center(tostring(59-g.at),db.x,db.y+30,db.w,db.h-35,20)
 local draw=option(options,'Draw two');if draw then hit(db,'act',draw)end
 for p=1,2 do local action=option(options,'Take pile '..p)or option(options,'Discard to '..p)or option(options,'Dig pile '..p);pile(M,g,p,{x=piles.x+p*(pw+7),y=piles.y,w=pw,h=piles.h},action)end
 local list={};local decision=g.ui==3 or g.ui==4 or g.step==1 or g.step==2 or g.step==5
 if decision then V.tab=0 end
 if g.ui==3 or g.ui==4 then for i,s in ipairs(options)do local c=tonumber(s:match('#(%d+)$'));if c then list[#list+1]={c=c,index=i}end end
 elseif g.step==1 then for _,c in ipairs(g.drawn)do if c>0 then list[#list+1]={c=c,index=cardOption(options,c)}end end
 elseif g.step==2 and g.pending>0 then list[1]={c=g.pending}
 elseif g.step==5 then for i,s in ipairs(options)do local c=tonumber(s:match('#(%d+)$'));if c then list[#list+1]={c=c,index=i}end end
 else for c=1,58 do local show=V.tab==0 and g.place[c]==1 or V.tab==1 and g.place[c]==3 or V.tab==2 and (g.place[c]==4 or g.place[c]==2 and (g.phase>=2 or(g.revealed&2)~=0))
  if show then list[#list+1]={c=c}end
 end end
 for i,title in ipairs({decision and(g.step==1 and'KEEP'or g.step==2 and'PLACE'or g.step==5 and'DIG'or'CHOOSE')or'HAND','YOURS','THEIRS'})do local b={x=tabs.x+(i-1)*tabs.w//3,y=tabs.y,w=tabs.w//3,h=tabs.h};rect(b,i-1==V.tab,false);label(title,b,10,i-1==V.tab);if not decision or i==1 then hit(b,'tab',i-1)end end
 V.tiles=list
 local pages=math.max(1,(#list+15)//16);V.page=math.max(1,math.min(V.page,pages));V.pages=pages
 local count=math.min(16,#list-(V.page-1)*16);local rows=math.max(1,(count+3)//4);local cw=(grid.w-21)//4;local ch=math.min(125,(grid.h-4-(rows-1)*7)//rows)
 for index=1,count do local entry=list[(V.page-1)*16+index];card(M,g,entry.c,{x=grid.x+(index-1)%4*(cw+7),y=grid.y+(index-1)//4*(ch+7),w=cw,h=ch},entry.index)end
 if count==0 then F.center('NO CARDS HERE',grid.x,grid.y+20,grid.w,28,10)end
 if pages>1 then F.center(V.page..' / '..pages,grid.x,grid.y+grid.h-18,grid.w,18,10)end
 local hintText=notice~=''and notice or V.selected and hints[kinds[V.selected]+1]or g.turn==2 and'THE OPPONENT IS TAKING A TURN.'or g.step==1 and'TAP THE CARD YOU WANT TO KEEP.'or g.step==2 and'TAP A PILE TO PLACE THE OTHER CARD.'or g.ui==3 and'CHOOSE THE FIRST CARD OF YOUR DUO.'or g.ui==4 and'CHOOSE ITS PARTNER.'or'DRAW TWO OR TAKE A PILE. SELECT A CARD TO READ ITS RULE.'
 hint(hb,hintText)
 local index=option(options,'End turn')or option(options,'Play duo (choose two cards)')or option(options,'Back')or option(options,'Cancel')
 local pair=V.selected and V.second and g.turn==1 and g.step==3
 local text=index and options[index]or g.turn==2 and'OPPONENT TURN'or'SELECT A CARD'
 if g.ui==3 or g.ui==4 then text='CANCEL';index=#options end
 if pair then button('PLAY DUO',primary,'duo',{V.selected,V.second},true)
 elseif index then button(string.upper(text),primary,'act',index,true)else rect(primary,false);label(text,primary,10)end
 local points=M.points(g,1);if option(options,'STOP')then button(points..' - CALL IT',call,'call',nil,false)else rect(call,false);label(points..' PTS',call,20)end
end
function V.draw(M,g,cursor,notice)
 controls={};context={M=M,g=g,options=select(2,M.view(g))};local w,h=smudge.get_bounds();smudge.clear()
 if V.help>0 then tutorial(M,g,w,h)
 elseif V.menu then drawMenu(M,g,context.options,w,h)
 elseif g.ui==9 then header(w,'NEW GAME');F.center('REPLACE THIS MATCH?',16,150,w-32,60,20);button('CANCEL',{x=16,y=h-140,w=w-32,h=52},'act',1);button('NEW GAME',{x=16,y=h-72,w=w-32,h=52},'act',2,true)
 elseif V.call then header(w,'CALL IT',M.points(g,1)..' PTS');local half=(h-135)//2;button('STOP',{x=16,y=119,w=w-32,h=half-12},'act',option(context.options,'STOP'));button('LAST CHANCE',{x=16,y=119+half,w=w-32,h=half-12},'act',option(context.options,'LAST CHANCE'))
 elseif g.phase>=2 and g.ui~=1 then round(M,g,context.options,w,h)
 else drawBoard(M,g,context.options,w,h,notice)end
 V.focus=math.max(1,math.min(V.focus,#controls));V.controls=controls
end
local function invoke(control)
 if not control then return end
 local op,value=control.op,control.value
 if op=='act'or op=='duo'then V.saved=true;V.call=false;V.selected=nil;V.second=nil;V.focus=1;return value
 elseif op=='menu'then if V.help>0 then V.help=0 elseif V.call then V.call=false else context.M.pause(context.g);V.selected=nil;V.menu=true end
 elseif op=='continue'then V.menu=false;V.focus=1
 elseif op=='new'then V.menu=false;return option(context.options,'New match')
 elseif op=='help'then V.help=1;V.focus=1
 elseif op=='lesson'then V.help=V.help==6 and 0 or V.help+1
 elseif op=='tab'then V.tab=value;V.page=1;V.selected=nil
 elseif op=='call'then V.call=true;V.focus=1
 elseif op=='select'then
  local g=context.g;local a=V.selected;local x,y=a and kinds[a],kinds[value]
  if a==value then V.selected=nil;V.second=nil
  elseif a and g.place[a]==1 and g.place[value]==1 and ((x==y and x<=2)or(x==3 and y==4)or(x==4 and y==3))then V.second=value
  else V.selected=value;V.second=nil end
 end
end
function V.touch(x,y)if y<83 then return invoke({op='menu'})end;for _,control in ipairs(controls)do local b=control.box;if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then return invoke(control)end end end
function V.focused()return invoke(controls[V.focus])end
function V.button(b)
 if b=='up'or b=='down'then V.focus=(V.focus+(b=='up'and -2 or 0))%math.max(1,#controls)+1;return true
 elseif b=='left'or b=='page_back'then V.pageBy(-1);return true elseif b=='right'or b=='page_forward'then V.pageBy(1);return true end
 return false
end
function V.pageBy(delta)V.page=math.max(1,math.min(V.pages or 1,V.page+delta))end
function V.playing()return not V.menu and V.help==0 and not V.call end
return V
