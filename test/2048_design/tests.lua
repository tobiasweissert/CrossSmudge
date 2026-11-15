local i,n=abi();assert(i==4 and n==4,'must use shipped int32/float32 Lua')
local H=smudge.dofile('test/2048_design/support.lua')
local w,h=smudge.get_bounds();local suffix=w..'x'..h
local reference={board=H.flat({2,4,8,16,0,2,32,64,0,0,4,128,0,0,2,256}),score='2136',best_score='5420',game_over='0'}
local app,s=H.app('main.lua','sticky',reference)
app.on_draw();capture('reference-'..suffix)
app.on_exit();H.equal(s,reference);assert(s.best_score=='5420' and s.best_score_backup=='5420')
local ui=app.smudge.dofile('ui.lua');ui.init();local g=ui.geometry(w,h)
assert(g.bh>=44 and g.bw>=44 and g.x>=0 and g.y>=80)
assert(g.y+4*g.cell+3*g.gap<=g.by and g.bx+g.bw<=w and g.by+g.bh<=h)
assert(ui.hit(g.bx-1,g.by,false,w,h)==nil)
app.on_tap(g.bx+g.bw/2,g.by+g.bh/2);app.on_draw();capture('dialog-'..suffix)
app.on_swipe('left');app.on_exit();H.equal(s,reference) -- modal blocks movement
app.on_button('back',true);app.on_exit();H.equal(s,reference);assert(not app.exited)
app.on_tap(g.bx+10,g.by+10)
app.on_tap(math.floor((w-360)/2)+266,math.floor((h-180)/2)+136)
app.on_exit();assert(s.score=='0' and s.best_score=='5420')
local count=0 for v in s.board:gmatch('[^,]+') do if v~='0' then count=count+1 end end assert(count==2)
app.on_draw();capture('new-game-'..suffix)
local reload,rs=H.app('main.lua','sticky',s);reload.on_exit();H.equal(rs,s);assert(rs.best_score=='5420')
-- No invented historical Best; current score seeds a missing or corrupt history.
for _,value in ipairs({'','oops','-1','nan','2147483648'}) do
 local e,t=H.app('main.lua','sticky',{board=reference.board,score='2136',best_score=value})
 e.on_exit();assert(t.best_score=='2136')
end
local e,t=H.app('main.lua','sticky',{board=reference.board,score='12',best_score='oops',best_score_backup='9000'})
e.on_exit();assert(t.best_score=='9000')
e.fail_save=true;e.on_button('confirm',true);e.on_button('confirm',true);e.on_exit();assert(t.best_score=='9000')
e.fail_save=false;e.on_exit();assert(t.best_score=='9000')
local f,fs=H.app('main.lua','sticky',{board=reference.board,score='2136'})
f.fail_save=true;f.on_exit();assert(f.last_log and fs.score=='2136' and fs.board==reference.board)
f.fail_save=false;f.on_exit();assert(fs.best_score=='2136')
-- Core differential: all directions, random boards, no-op, single merge,
-- terminal boards, large values, and repeated shuffles; identical RNG seeds.
local boards={{2,2,2,2},{2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536},
 {32768,32768,65536,65536},{2,4,2,4,4,2,4,2,2,4,2,4,4,2,4,2},{}}
math.randomseed(40)
for b=1,25 do local a={} for j=1,16 do a[j]=({0,0,2,4,8,16})[math.random(6)] end boards[#boards+1]=a end
for _,board in ipairs(boards) do
 for _,dir in ipairs({'left','right','up','down'}) do
  local initial={board=H.flat(board),score='100',game_over='0'}
  local old,os=H.app('test/2048_design/baseline.lua','sticky',initial)
  local new,ns=H.app('main.lua','sticky',initial)
  for step=1,12 do
   math.randomseed(1000+step);old.on_swipe(dir);old.on_exit()
   math.randomseed(1000+step);new.on_swipe(dir);new.on_exit()
   H.equal(os,ns)
  end
 end
end
local late,ls=H.app('main.lua','sticky',{board=H.flat({131072,65536,32768,16384,8192,4096,2048,1024,512,256,128,64,32,16,8,4}),score='2147483647',best_score='2147483647',game_over='1'})
late.on_draw();capture('late-game-'..suffix)
late.on_swipe('left');late.on_exit();assert(ls.score=='2147483647')
local empty=H.app('main.lua','sticky',{board=H.flat({}),score='0'})
empty.on_draw();capture('empty-'..suffix)
print('PASS: ABI; reference; geometry; touch; reset/back; real Best; readback failures; reload; 1440 differential moves; late digits')
-- Invalid records recover to a playable new game without inventing Best.
for _,bad in ipairs({'bad','1,2,3',H.flat({-2}),H.flat({3}),H.flat({0/0})}) do
 local e,s=H.app('main.lua','sticky',{board=bad,score='oops',best_score='500'})
 e.on_draw();e.on_exit();assert(s.score=='0' and s.best_score=='500')
end
local full={board=H.flat({2,4,2,4,4,2,4,2,2,4,2,4,4,2,4,2}),score='12',game_over='0'}
local no,ns=H.app('main.lua','sticky',full);no.on_swipe('left');no.on_exit();H.equal(full,ns)
local merge,ms=H.app('main.lua','sticky',{board=H.flat({2,2,2,2}),score='0'})
merge.on_swipe('left');merge.on_exit();assert(ms.score=='8' and ms.board:match('^4,4,'))
for idx,dir in ipairs({'up','down','left','right'}) do
 local a,sa=H.app('main.lua','sticky',reference)
 local b,sb=H.app('main.lua','sticky',reference)
 math.randomseed(9);a.on_swipe(dir);a.on_exit()
 math.randomseed(9);b.on_button(({'left','right','up','down'})[idx],true);b.on_exit()
 H.equal(sa,sb)
end
print('PASS: corrupt storage; explicit no-spawn no-op; single merges; button/swipe mappings')
local decimal,ds=H.app('main.lua','sticky',{board='2.0,4.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0',score='0'})
decimal.on_draw();decimal.on_exit();assert(ds.board:match('^2,4,'))
local boundary={board=H.flat({1073741824,1073741824,2,2}),score='2147483647',game_over='0'}
local max,ms=H.app('main.lua','sticky',boundary)
max.on_draw();capture('max-values-'..suffix)
local base,bs=H.app('test/2048_design/baseline.lua','sticky',boundary)
math.randomseed(17);max.on_swipe('left');max.on_exit();max.on_draw();capture('overflow-'..suffix)
math.randomseed(17);base.on_swipe('left');base.on_exit();H.equal(ms,bs)
assert(ms.board:match('^-2147483648,'))
print('PASS: decimal board normalization; max tile fitting; original int32 overflow differential without draw crash')

local scoreOverflow, ss=H.app('main.lua','sticky',{board=H.flat({2,2}),score='2147483647'})
scoreOverflow.on_swipe('left');scoreOverflow.on_exit();scoreOverflow.on_draw()
assert(ss.score=='-2147483645')
