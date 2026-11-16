local app=assert(os.getenv('BOARD_APP'));local M=dofile('core.lua');local F=dofile('graphics.lua');local B=dofile('board.lua');local w,h=smudge.get_bounds()
local expected=M.new(smudge.millis());dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
local function tap(x,y)on_touch(x,y,'tap');on_draw()end
if app=='go'then
 for _,n in ipairs({9,13,19})do local g=M.new(1,n);assert(M.play(g,n+2));F.init();local l=B.draw(M,g,nil,F,w,h);assert(B.hit(l,n,l.x0+l.pitch,l.y0+l.pitch)==n+2);assert(not B.hit(l,n,-1,-1));M.play(g,0);M.play(g,0);B.draw(M,g,nil,F,w,h);M.mark(g,n+2);B.draw(M,g,nil,F,w,h);M.accept(g,1);M.accept(g,2);B.draw(M,g,nil,F,w,h)end
 local l=B.layout(w,h,9);local x,y=l.x0+4*l.pitch,l.y0+4*l.pitch
 tap(x,y);on_exit();local aimed=dofile('storage.lua').load(M.decode);assert(aimed.moves==0,'first aiming tap played a stone')
 tap(x,y);assert(M.play(expected,41))
elseif app=='toybattle'then
 F.init();for map=0,9 do local g=M.new(1,map);local l=B.draw(M,g,nil,nil,F,w,h);for i,p in ipairs(l.points)do local kind,id=B.hit(l,p[1],p[2]);assert(kind=='slot'and id==i,'terrain hit mismatch '..map..':'..i)end end
 local move;for _,m in ipairs(M.moves(expected))do if #m>0 then move=m;break end end;assert(move)
 local p,k=move[1][1],move[1][2];local l=B.draw(M,expected,nil,nil,F,w,h);local die
 for i,kind in ipairs(l.kinds)do if kind==k then die=l.rack[i];break end end;assert(die)
 tap(die.x+die.w//2,die.y+die.h//2);tap(l.points[p][1],l.points[p][2])
 M.act(expected,2);expected.edit[1]=p;expected.edit[2]=k
 for i=1,9 do on_button('down',true)end;on_button('confirm',true);on_draw();M.act(expected,10)
end
on_touch(-1,-1,'tap');on_touch(w,h,'tap');on_exit()
local loaded=dofile('storage.lua').load(M.decode);assert(loaded and M.encode(loaded)==M.encode(expected),'graphical touch/core journal mismatch '..app)
print('Graphical board/aim-draft/core/journal/count-result checks passed: '..app)

if app=='toybattle'then
 local H=dofile('lesson.lua');F.init();for page=1,8 do local boxes=H.draw(page,F,w,h,B);assert(#boxes==2)end
 on_button('back',true);on_draw();on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw()
 for page=1,8 do on_button('confirm',true);on_draw()end
 print('Original eight-page walkthrough / front-door navigation passed')
end
