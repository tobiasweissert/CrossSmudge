return function(app,name)
 local g=app.state();local M=dofile('core.lua');local a,ca=M.points(g,1);local b,cb=M.points(g,2);local mermaids=M.count(g,1,9,true)==4 or M.count(g,2,9,true)==4;local deck=g.ender==0 and not mermaids;local won=g.ender==1 and a>=b or g.ender==2 and b>=a
 local bank={};for seat=1,2 do local p,c=seat==1 and a or b,seat==1 and ca or cb;bank[seat]=deck and 0 or g.bet==1 and(seat==g.ender and(won and p+c or c)or(won and c or p))or p end
 local winner=M.count(g,1,9,true)==4 or M.count(g,2,9,true)~=4 and(g.score[1]>g.score[2]or g.score[1]==g.score[2]and g.starter==2)
 local n={g.round,g.ender==1 and 1 or 0,g.bet,won and 1 or 0,deck and 1 or 0,mermaids and 1 or 0,winner and 1 or 0,a,ca,bank[1],b,cb,bank[2],g.score[1],g.score[2],g.phase==3 and 1 or 0};assert(smudge.write_file(name..'.round',table.concat(n,' ')..'\n',false))
end
