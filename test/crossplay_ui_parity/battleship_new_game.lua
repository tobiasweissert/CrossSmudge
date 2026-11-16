-- Source counts are based on actual core completions, with production startup,
-- draw/save/reopen callbacks. No synthetic stat values or puzzle setters.
local read=smudge.read_file;local saved={};local reject=false
smudge.read_file=function(path,cap,offset)
 if saved[path]then return saved[path]:sub((offset or 0)+1,(offset or 0)+cap)end
 if path:match('%.dat$')then return nil end
 return read(path,cap,offset)
end
smudge.write_file=function(path,value)if reject and path:match('^native%-stats')then return false end;saved[path]=value;return true end
local M=dofile('core.lua')
local function completed(seed)
 local g=M.new(seed);for row=1,5 do assert(M.action(g,1,row))end
 for cell=1,100 do if g.b:sub(cell,cell)~='0'then
  assert(M.fire(g,cell))
  if g.status==0 then for water=1,100 do if g.a:sub(water,water)=='0'and g.sa:sub(water,water)=='0'then assert(M.fire(g,water));break end end end
 end end
 assert(g.status==1 and M.valid(g));return g
end
assert(dofile('storage.lua').save('1|'..M.encode(completed(123456))))
local app=dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw()
assert(app.state().phase==0 and app.layout().placement,'native New Game from completed menu did not open full fleet directly')
assert(app.records().played==1,'New Game lost observed completion')
on_exit()
