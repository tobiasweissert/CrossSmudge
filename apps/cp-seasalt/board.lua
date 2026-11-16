local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_board.lua')end
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
local function box(x,y,w,h)return{x=x,y=y,w=w,h=h}end
local function button(text,b,op,value,filled)
 F.button(text,b,filled or false,false,filled or false);if op then hit(b,op,value)end
end
local function capsule(text,b,op,value,enabled)
 if enabled then rect(b,true)else smudge.rect_dither(b.x,b.y,b.w,b.h,true)end
 F.button(text,b,nil,false,true);if op and enabled then hit(b,op,value)end
end
local function header(w,title,right)
 smudge.rect(0,0,w,76,true,true);F.text(title,16,19,30,true)
 if right then F.native(right,box(16,27,w-32,42),20,'right',true)end
 smudge.rect(0,80,w,3,true,true)
end
local function option(options,needle)for i,s in ipairs(options)do if s==needle then return i end end end
local function cardOption(options,c)for i,s in ipairs(options)do if tonumber(s:match('#(%d+)$'))==c then return i end end end
function V.init(saved,record)
 F.init();V.saved=saved;V.record=record or{played=0,won=0};V.nearby=assert(smudge.read_file('art/nearby32.raw',128));V.unavailable=false;V.report=''
 for k,name in ipairs(artnames)do icons[k]={};for _,size in ipairs({24,40,48})do icons[k][size]=assert(smudge.read_file('art/ss_'..name..'_'..size..'.raw',400))end end
 for i,name in ipairs(marknames)do marks[i]={};for _,size in ipairs({20,24})do marks[i][size]=assert(smudge.read_file('art/ss_m_'..name..(size==20 and'_native20'or'_24')..'.raw',72))end end
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
V.groupPoints=groupPoints
local function card(M,g,c,b,action,example)
 local kind=example and example.kind or kinds[c];rect(b,true,true);rect(b,false,false,2)
 if not example and(V.selected==c or V.second==c)then smudge.rect(b.x-3,b.y-3,b.w+6,b.h+6,false,2,true)end
 local n=math.max(10,math.min(20,b.h//4));colour(example and example.colour or colours[c],b.x+7,b.y+7)
 local points=tostring(example and example.points or groupPoints(M,g,kind,(g.place[c]==2 or g.place[c]==4)and 2 or 1));F.nativeLabel(points,box(b.x,b.y+5,b.w-9,24),20,'right')
 local census=b.y+b.h-21;local nameY=census-19;local named=nameY-(b.y+28)>=40
 local faceTop=named and b.y+28 or b.y+20;local room=(named and nameY or census)-faceTop
 local size=room>=48 and 48 or 40
 picture(kind,b.x+(b.w-size)//2,faceTop+((room-size)<0 and -((size-room)//2)or(room-size)//2),size)
 if named then F.nativeLabel(names[kind+1],box(b.x,nameY,b.w,15),10,'center')end
 F.nativeLabel('X'..supply[kind+1],box(b.x,census,b.w,15),10,'center')
 if not example then if action then hit(b,'act',action)elseif g.turn==1 and g.step==3 and V.tab==0 then hit(b,'select',c)end end
end
local function pile(M,g,p,b,action)
 rect(b,true,true);rect(b,false,false,2);local c=M.top(g,p);local count=0;for i=1,58 do if g.place[i]==p+4 then count=count+1 end end
 if c==0 then F.native('EMPTY',b,10,'center')else
  colour(colours[c],b.x+6,b.y+6);F.nativeLabel(tostring(count),box(b.x,b.y+5,b.w-8,24),20,'right')
  local size=b.h>=85 and 40 or 24;picture(kinds[c],b.x+(b.w-size)//2,b.y+26,size)
  F.nativeLabel(names[kinds[c]+1]..' X'..supply[kinds[c]+1],box(b.x,b.y+b.h-23,b.w,16),10,'center')
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
 local dot=text:find('%. ')
 if dot then local half=b.h//2;F.native(text:sub(1,dot),box(b.x,b.y+3,b.w,half-3),10,'center');F.native(text:sub(dot+2),box(b.x,b.y+half,b.w,half-3),10,'center')else F.native(text,b,10,'center')end
end
local function drawMenu(M,g,options,w,h)
 header(w,'SEA SALT');F.native(V.record.played..' PLAYED   '..V.record.won..' WON',box(16,119,w-32,26),10,'left');smudge.rect(16,151,w-32,3,true,true)
 local rows={};if V.saved and g.phase~=3 then rows[#rows+1]={'CONTINUE','continue','ROUND '..g.round..', '..g.score[1]..'-'..g.score[2]}end
 rows[#rows+1]={'NEW GAME','new'};rows[#rows+1]={'PLAY NEARBY','nearby'};rows[#rows+1]={'HOW TO PLAY','help'}
 local count=#rows;local top=h-16-(count*62+(count-1)*6+12)
 local colW=(w-32)//2;local artTop=145+(top-145-7*44)//2
 for kind=0,13 do local col=kind//7;local x=16+col*colW+(col==1 and 14 or 0);local y=artTop+(kind%7)*44
  picture(kind,x,y+10,24);F.native(names[kind+1],box(x+32,y,colW-96,44),10,'left');F.native('X'..supply[kind+1],box(x+colW-64,y,50-(col==1 and 14 or 0),44),10,'right')
 end
 for i,row in ipairs(rows)do local b=box(16,top+(i-1)*66,w-32,62);rect(b,i==V.focus);F.native(row[1],box(b.x+12,b.y,b.w-24,b.h),20,'left',i==V.focus);if row[3]then F.native(row[3],box(b.x+12,b.y,b.w-24,b.h),20,'right',i==V.focus)end
  if row[2]=='nearby'then smudge.bitmap(b.x+b.w-56,b.y+15,32,32,V.nearby,i==V.focus)end;hit(b,row[2])
 end
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
 F.nativeLabel(page.title,box(16,at(16),w-32,26),20,'center')
 for _,line in ipairs(page.lines)do F.native(line[2],box(16,at(line[1]),w-32,22),10,'center')end
 if page.caption then F.nativeLabel(page.caption[2],box(16,at(page.caption[1]),w-32,26),20,'center')end
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
 header(w,g.phase==3 and'SEA SALT'or'ROUND '..g.round)
 local a,ca=M.points(g,1);local b,cb=M.points(g,2);local mermaid=M.count(g,1,9,true)==4 or M.count(g,2,9,true)==4;local deckOut=g.ender==0 and not mermaid
 local betWon=g.ender==1 and a>=b or g.ender==2 and b>=a;local headline
 if mermaid then headline=M.count(g,1,9,true)==4 and'FOUR MERMAIDS. YOU WIN.'or'FOUR MERMAIDS. THEY WIN.'
 elseif deckOut then headline='THE DECK RAN OUT. NOBODY SCORES.'elseif g.bet==0 then headline=g.ender==1 and'YOU CALLED STOP.'or'THEY CALLED STOP.'
 else headline=(g.ender==1 and'YOUR BET 'or'THEIR BET ')..(betWon and'CAME OFF.'or'FAILED.')end
 F.native(headline,box(16,119,w-32,34),20,'center');local y=159
 if not deckOut and not mermaid then
  for seat=1,2 do local points,bonus=seat==1 and a or b,seat==1 and ca or cb;local banked=points
   if g.bet==1 then banked=seat==g.ender and(betWon and points+bonus or bonus)or(betWon and bonus or points)end
   F.native((seat==1 and'YOU 'or'THEM')..'  CARDS '..points..'  BONUS '..bonus..'  BANKED '..banked,box(16,y,w-32,30),10,'left');y=y+30
  end;y=y+6
 end
 F.native(g.score[1]..' - '..g.score[2],box(16,y,w-32,40),30,'center')
 capsule(g.phase==3 and'PLAY AGAIN'or'NEXT ROUND',box(16,h-68,w-32,52),'act',g.phase==2 and 1 or 3,true)
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
  F.native(tostring(pair[1]),box(x+8,facts.y,28,44),20,'left');F.native(pair[2],box(x+40,facts.y+2,fw-44,20),10,'left');F.native(pair[3],box(x+40,facts.y+22,fw-44,20),10,'left')
 end
 for i=1,3 do smudge.rect(facts.x+i*fw,facts.y,1,facts.h,true,true)end
 local bx=facts.x+3*fw;F.native(tostring(best),box(bx+8,facts.y,28,44),20,'left');colour(bestColour,bx+32,facts.y+12,20);F.nativeLabel('BEST',box(bx+58,facts.y+14,fw-62,16),10,'left')
 local pw=(piles.w-14)//3;local db={x=piles.x,y=piles.y,w=pw,h=piles.h};smudge.rect_dither(db.x,db.y,db.w,db.h,false);rect(db,false,false,2);F.native('DECK',box(db.x,db.y+8,db.w,40),10,'center');F.native(tostring(59-g.at),box(db.x,db.y+48,db.w,40),20,'center')
 local draw=option(options,'Draw two');if draw then hit(db,'act',draw)end
 for p=1,2 do local action=option(options,'Take pile '..p)or option(options,'Discard to '..p)or option(options,'Dig pile '..p);pile(M,g,p,{x=piles.x+p*(pw+7),y=piles.y,w=pw,h=piles.h},action)end
 local list={};local decision=g.ui==3 or g.ui==4 or g.step==1 or g.step==2 or g.step==5
 if decision then V.tab=0 end
 if g.ui==3 or g.ui==4 then for i,s in ipairs(options)do local c=tonumber(s:match('#(%d+)$'));if c then list[#list+1]={c=c,index=i}end end
 elseif g.step==1 then for _,c in ipairs(g.drawn)do if c>0 then list[#list+1]={c=c,index=cardOption(options,c)}end end
 elseif g.step==2 and g.pending>0 then list[1]={c=g.pending}
 elseif g.step==5 then for i,s in ipairs(options)do local c=tonumber(s:match('#(%d+)$'));if c then list[#list+1]={c=c,index=i}end end
 else for c=1,58 do local show=V.tab==0 and g.place[c]==1 or V.tab==1 and g.place[c]==3 or V.tab==2 and (g.place[c]==4 )
  if show then list[#list+1]={c=c}end
 end end
 rect(tabs,false,false,2)
 local counts={0,yours,theirs};for c=1,58 do if g.place[c]==1 then counts[1]=counts[1]+1 end end
 for i=1,3 do local title=({'HAND','YOURS','THEIRS'})[i]..' '..counts[i]
  if i==1 and decision then title=g.step==1 and'KEEP'or g.step==2 and'PLACE'or g.step==5 and'DIG'or'CHOOSE'end
  local b=box(tabs.x+(i-1)*(tabs.w//3),tabs.y,i==3 and tabs.w-2*(tabs.w//3)or tabs.w//3,tabs.h)
  if i-1==V.tab then rect(b,true)elseif i>1 then smudge.rect(b.x,b.y,1,b.h,true,true)end;F.native(title,b,10,'center',i-1==V.tab);hit(b,'tab',i-1)
 end
 V.tiles=list
 local pages=math.max(1,(#list+15)//16);V.page=math.max(1,math.min(V.page,pages));V.pages=pages
 local count=math.min(16,#list-(V.page-1)*16);local rows=math.max(1,(count+3)//4);local cw=(grid.w-21)//4;local ch=math.min(125,(grid.h-4-(rows-1)*7)//rows)
 for index=1,count do local entry=list[(V.page-1)*16+index];card(M,g,entry.c,{x=grid.x+(index-1)%4*(cw+7),y=grid.y+(index-1)//4*(ch+7),w=cw,h=ch},entry.index)end

 if pages>1 then F.nativeLabel(V.page..' / '..pages,box(grid.x,grid.y+grid.h-16,grid.w,14),10,'right')end
 local x,y=V.selected and kinds[V.selected],V.second and kinds[V.second];local pair=x and y and((x==y and x<=2)or(x==3 and y==4)or(x==4 and y==3))and g.turn==1 and g.step==3
 local hintText,text,index,enabled
 if g.turn~=1 then hintText='THEIR TURN.';text='THEIR TURN'
 elseif g.step==0 then hintText=V.report and V.report~=''and V.report or'TAKE A CARD. DECK DEALS TWO, PILES SHOW WHAT YOU GET.';text='TAKE A CARD'
 elseif g.step==1 then hintText='TWO FROM THE DECK. TAP THE ONE YOU KEEP.';text='KEEP ONE'
 elseif g.step==2 then hintText='THE '..names[kinds[g.pending]+1]..' GOES FACE UP. TAP A PILE ABOVE.';text='PLACE IT'
 elseif g.step==4 then hintText='THE CRABS DIG. TAP A PILE ABOVE TO SEARCH IT.';text='DIG'
 elseif g.step==5 then hintText='TAKE ANY ONE CARD FROM THIS PILE.';text='TAKE ONE'
 else
  hintText=V.selected and V.second and not pair and'THESE TWO DO NOT PAIR. A PAIR IS TWO ALIKE, OR SWIMMER AND SHARK.'or pair and({'TWO CRABS. PLAY THEM TO DIG THROUGH A PILE.','TWO BOATS. PLAYING THEM BUYS ANOTHER TURN.','TWO FISH. PLAY THEM TO DRAW A CARD.','SWIMMER AND SHARK. PLAY THEM TO STEAL A CARD.','SWIMMER AND SHARK. PLAY THEM TO STEAL A CARD.'})[kinds[V.selected]+1]or V.selected and hints[kinds[V.selected]+1]or V.tab~=0 and'PAIRS LAID HERE ALREADY DID THEIR WORK. POINTS COUNT EITHER WAY.'or V.report and V.report~=''and V.report or'TAP A CARD TO SEE WHAT IT DOES. END THE TURN WHEN DONE.'
  text=pair and'PLAY THE PAIR'or'END TURN';index=option(options,'End turn');enabled=index~=nil or pair
 end
 if notice~=''then hintText=notice end;hint(hb,hintText)
 capsule(text,primary,pair and'duo'or'act',pair and{V.selected,V.second}or index,enabled)
 local points=M.points(g,1);local canCall=option(options,'STOP')~=nil;capsule(points..(canCall and' - CALL IT'or' PTS'),call,'call',nil,canCall)
 V.renderModel={hint=hintText,primary=text,primaryEnabled=enabled or false,callPoints=points,canCall=canCall,handCount=counts[1],yoursCount=yours,theirsCount=theirs,page=V.page,pages=pages,handTabLabel=decision and(g.step==1 and'KEEP'or g.step==2 and'PLACE'or g.step==5 and'DIG'or'CHOOSE')or'',theyHold=held,bestColour=bestColour,bestColourCount=best,tiles=list}

end
function V.draw(M,g,cursor,notice)
 controls={};context={M=M,g=g,options=select(2,M.view(g))};local w,h=smudge.get_bounds();smudge.clear()
 if V.help>0 then tutorial(M,g,w,h)
 elseif V.unavailable then header(w,'PLAY NEARBY');F.native('NEARBY PLAY IS UNAVAILABLE ON THIS DEVICE.',box(16,230,w-32,120),20,'center',false,4);capsule('BACK',box(16,h-68,w-32,52),'closeNearby',nil,true)
 elseif V.menu then drawMenu(M,g,context.options,w,h)
 elseif g.ui==9 then header(w,'NEW GAME');F.center('REPLACE THIS MATCH?',16,150,w-32,60,20);button('CANCEL',{x=16,y=h-140,w=w-32,h=52},'act',1);button('NEW GAME',{x=16,y=h-72,w=w-32,h=52},'act',2,true)
 elseif V.call then header(w,'CALL IT',M.points(g,1)..' PTS');local half=(h-147)//2
  for i,title in ipairs({'STOP','LAST CHANCE'})do local b=box(16,119+(i-1)*(half+12),w-32,half);rect(b,false,false,2)
   F.native(title,box(b.x,b.y+16,b.w,48),30,'center')
   F.native(i==1 and'EVERYBODY BANKS THEIR CARDS.'or'BET YOU HAVE THE MOST. WIN: CARDS PLUS BONUS.',box(b.x+12,b.y+76,b.w-24,22),10,'center')
   F.native(i==1 and'SAFE, AND IT PAYS NO COLOUR BONUS.'or'LOSE: YOU BANK ONLY YOUR BONUS.',box(b.x+12,b.y+100,b.w-24,22),10,'center');hit(b,'act',option(context.options,title))
  end
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
 elseif op=='new'then local had=V.saved;V.menu=false;V.saved=true;if had then return option(context.options,'New match')end
 elseif op=='nearby'then V.menu=false;V.unavailable=true
 elseif op=='closeNearby'then V.menu=true;V.unavailable=false
 elseif op=='help'then V.help=1;V.focus=1
 elseif op=='lesson'then V.help=V.help==6 and 0 or V.help+1
 elseif op=='tab'then V.tab=value;V.page=1;V.selected=nil;V.second=nil
 elseif op=='call'then V.call=true;V.focus=1
 elseif op=='select'then
  local g=context.g;local a=V.selected;local x,y=a and kinds[a],kinds[value]
  if a==value then V.selected=V.second;V.second=nil
  elseif V.second==value then V.second=nil
  elseif not a then V.selected=value
  elseif not V.second then V.second=value
  else V.selected=V.second;V.second=value end
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
function V.playing()return not V.menu and V.help==0 and not V.call and not V.unavailable end
return V
