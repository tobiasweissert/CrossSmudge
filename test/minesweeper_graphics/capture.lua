local M=dofile('core.lua');local g=M.new(123)
M.new=function()return g end
collectgarbage('collect');dofile('ui.lua').install(M);on_init()
on_draw();capture('menu.pgm')
on_button('confirm',true);on_touch(479,799,'tap')
-- Reference picture's covered/open arrangement; actual numbers from actual mines.
local rows={'......oo','.....ooo','ooo..ooo','oooooooo','oooooooo','.ooooooo','..oooooo','..oooo..','..oooo..','..oooo..'}
local mines={21,63,20,50,6,41,66,9,64,79}
for _,i in ipairs(mines)do g.b[i]=1 end
g.status=1
for r=1,10 do for c=1,8 do if rows[r]:sub(c,c)=='o' then g.b[(r-1)*8+c]=2 end end end
on_draw();capture('board.pgm')
g.b[2]=4;on_draw();capture('flag.pgm')
assert(M.action(g,1,2,false));assert(g.status==3);on_draw();capture('lost.pgm')
for i=1,80 do if g.b[i]&1==0 then g.b[i]=2 else g.b[i]=1 end end
g.status=2;on_draw();capture('won.pgm')
on_button('right',true);on_draw();capture('cursor.pgm')
on_button('back',true);on_button('down',true);on_button('down',true);on_button('down',true);on_button('confirm',true);on_draw();capture('help.pgm')

for page=2,5 do on_button('confirm',true);on_draw();capture('help'..page..'.pgm') end
on_button('back',true);on_button('confirm',true)
for n=1,8 do
 for i=1,80 do g.b[i]=0 end
 local neighbors=M.neighbors(28)
 for i=1,n do g.b[neighbors[i]]=1 end
 g.b[28]=2;g.status=1
 assert(M.adjacent(g,28,1)==n)
 on_touch(479,799,'tap');on_draw();capture('digit'..n..'.pgm')
end
