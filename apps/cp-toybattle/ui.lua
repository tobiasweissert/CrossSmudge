-- Original shell geometry over the existing map/score fields. No new engine state.
local U={};local function box(x,y,w,h)return{x=x,y=y,w=w,h=h}end
local function totalMedals(t)local n=0;for _,r in ipairs(t.regions)do n=n+r[2]end;return n end
function U.setup(M,g,F,B,w,h,options)
 F.header('NEW GAME');local model={map=options.map};local terrain=M.board(model);local controls={}
 for i,row in ipairs({{'MAP',terrain.name,'maps'},{'OPPONENT','LOCAL','local'},{'YOU MOVE','FIRST','first'},{'SPECIAL BASES',options.special and'ON'or'OFF','bases'}})do
  local b=box(16,119+(i-1)*66,w-32,62);smudge.rect(b.x,b.y,b.w,b.h,false,1,true);F.native(row[1],box(b.x+12,b.y,b.w-24,b.h),20,'left');F.native(row[2],box(b.x+12,b.y,b.w-24,b.h),20,'right');if row[3]=='maps'or row[3]=='bases'then controls[#controls+1]={box=b,op=row[3]}end
 end
 B.mini(M,model,F,box(16,397,w-32,267),false)
 F.native(terrain.goal..' MEDALS TO WIN, '..totalMedals(terrain)..' ON THE BOARD',box(16,666,w-32,24),10,'center')
 local b=box(16,h-92,w-32,76);smudge.rounded_rect(b.x,b.y,b.w,b.h,10,true,true,1);F.native('START',box(b.x+4,b.y+2,b.w-8,b.h-4),30,'center',true);controls[#controls+1]={box=b,op='start'};return controls
end
function U.maps(M,g,F,B,w,h,page)
 F.header('MAPS',(page+1)..'/2');local choices={0,2,3,4,5,6,7,8,9};local controls={}
 for row=1,5 do local at=page*5+row;local id=choices[at];if not id then break end;local terrain=M.board({map=id});local b=box(16,119+(row-1)*116,w-32,104)
  smudge.rounded_rect(b.x,b.y,b.w,b.h,8,true,false,1);smudge.rounded_rect(b.x,b.y,b.w,b.h,8,false,true,1)
  F.native(terrain.name,box(b.x+12,b.y+22,b.w-132,28),20,'left')
  F.native(terrain.n..' BASES   WIN AT '..terrain.goal..' OF '..totalMedals(terrain),box(b.x+12,b.y+58,b.w-132,24),10,'left')
  B.mini(M,{map=id},F,box(b.x+b.w-108,b.y+8,100,88),false);controls[#controls+1]={box=b,op='map',value=id}
 end
 for i=0,1 do local x=(w-34)//2+6+i*22;if i==page then F.disc(x,h-31,6,true)else F.disc(x,h-31,6,true);F.disc(x,h-31,5,false)end end
 return controls
end
local briefKey,briefPixels
function U.brief(M,g,F,w,h)
 F.header(M.board(g).name);local key=g.map..'-'..g.special
 if briefKey~=key then briefPixels=assert(smudge.read_file('art/brief-'..key..'.raw',37241));briefKey=key end
 smudge.bitmap(16,119,448,665,briefPixels);return{}
end
function U.result(M,g,F,w,h)
 local won=g.winner==1;F.header(won and'YOU WIN'or'YOU LOSE');local how
 if g.ending==1 then how=won and'YOU TOOK THEIR H.Q.'or'THEY TOOK YOUR H.Q.'elseif g.ending==2 then how=won and'YOU REACHED THE MEDALS'or'THEY REACHED THE MEDALS'else how='NOBODY COULD MOVE. MEDALS DECIDED IT'end
 F.native(how,box(16,119,w-32,28),10,'center');F.nativeLabel('MEDALS '..g.medals[1]..' - '..g.medals[2],box(16,159,w-32,60),30,'center')
 local controls={};for i,row in ipairs({{'DONE',h-68,'done'},{'PLAY AGAIN',h-132,'again'}})do local b=box(16,row[2],w-32,52);F.button(row[1],b,true,false,true);controls[i]={box=b,op=row[3]}end;return controls
end
function U.unavailable(F,w,h)
 F.header('PLAY NEARBY');F.native('NEARBY PLAY IS UNAVAILABLE ON THIS DEVICE.',box(16,230,w-32,120),20,'center',false,4);local b=box(16,h-68,w-32,52);F.button('BACK',b,true,false,true);return{{box=b,op='done'}}
end
return U
