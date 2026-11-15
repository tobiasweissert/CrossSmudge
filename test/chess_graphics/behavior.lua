smudge.get_device=function()return'sticky'end
local app=dofile('main.lua');on_init();local C,G=app.core,app.state();local N=dofile('notation.lua');on_draw();local l=app.layout()
local w,h=smudge.get_bounds();assert(l.y==140 and l.size==math.max(8,math.min(w-50,h-251)//8));assert(l.x==(w-l.size*8)//2)
local function san(fen,from,to,prom,expected)
 local p=assert(C.parseFen(fen));local before=C.fen(p);local found
 for _,m in ipairs(C.legal(p))do if m.from==from and m.to==to and(prom==nil or m.promotion==prom)then found=true;assert(N.san(C,p,m)==expected,expected..' mismatch');assert(C.fen(p)==before,'SAN mutated position')end end;assert(found)
end
san(C.fen(C.start()),12,28,nil,'e4')
san('r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1',4,6,nil,'O-O')
san('r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1',4,2,nil,'O-O-O')
san('7k/8/8/3pP3/8/8/8/K7 w - d6 0 1',36,43,nil,'exd6')
san('7k/P7/8/8/8/8/7p/K7 w - - 0 1',48,56,2,'a8=N')
san('7k/8/8/8/8/8/8/KN3N2 w - - 0 1',1,11,nil,'Nbd2')
san('7k/8/8/8/8/8/8/KN3N2 w - - 0 1',5,11,nil,'Nfd2')
local p=C.start();for i,pair in ipairs({{13,21},{52,36},{14,30},{59,31}})do for _,m in ipairs(C.legal(p))do if m.from==pair[1]and m.to==pair[2]then local before=C.fen(p);local text=N.san(C,p,m);assert(text==({'f3','e5','g4','Qh4#'})[i]);C.make(p,m);assert(N.between(C,before,C.fen(p))==text);break end end end
-- Settings open via the drawn gear, preserve FEN, and expose supported rows.
on_touch(w-20,38,'tap');assert(G.menu and not G.job);on_draw();local before=C.fen(G.position);local old=G.mode
local function target(i)local bb=assert(app.layout().buttons[i]);assert(bb.x>=0 and bb.y>=0 and bb.x+bb.w<=w and bb.y+bb.h<=h);on_touch(bb.x+bb.w//2,bb.y+bb.h//2,'tap');on_draw()end
for _,i in ipairs({1,2,3,4,5,6,7,8,9})do assert(app.layout().buttons[i],'setting missing '..i)end
target(8);assert(G.mode~=old and C.fen(G.position)==before);target(8);assert(G.mode==old);target(9);target(9);target(1);assert(not G.menu)
-- All four promotion choices are on-screen and select exactly the drawn type.
for i,kind in ipairs({5,4,3,2})do G.job=nil;G.mode=3;G.position=assert(C.parseFen('7k/P7/8/8/8/8/7p/K7 w - - 0 1'));G.moves=C.legal(G.position);G.over=nil;G.selected=nil;G.history={};G.promotion={};for _,m in ipairs(G.moves)do if m.from==48 and m.to==56 then G.promotion[#G.promotion+1]=m end end;G.promoIndex=i;on_draw();target(i);assert(G.position.squares[56]==kind)end
on_exit();assert(G.closed and not G.job);print('Original board geometry/settings/promotion; SAN castle, ambiguity, en passant, underpromotion and mate; position unchanged by notation')
