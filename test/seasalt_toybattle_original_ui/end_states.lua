assert(APP=='seasalt'and smudge.get_device()=='sticky');local M=dofile('core.lua');local U=dofile('util.lua');local cards=dofile('cards.lua');local kinds=cards[1];local app
local function tap(op,value)
 for _,c in ipairs(app.view.controls)do if c.op==op and(value==nil or c.value==value)then local b=c.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing '..op)
end
local function prepare(wanted)
 for seed=1,300 do local g=M.new(seed);if wanted=='deck'then M.deal(g,seed,2);g.round=2 end
  for guard=1,400 do
   if g.phase>=2 then break end
   local moves=M.moves(g,nil,true);local choice
   if g.step==0 then
    if wanted=='deck'then choice=moves[1]
    elseif g.turn==1 then for _,m in ipairs(moves)do if m[1]==2 and kinds[M.top(g,m[2])]==9 then choice=m;break end end;choice=choice or moves[1]
    else for _,m in ipairs(moves)do if m[1]==2 and kinds[M.top(g,m[2])]~=9 then choice=m;break end end;choice=choice or moves[1]end
   elseif g.step==1 then choice=moves[1];if wanted~='deck'and kinds[g.drawn[2]]==9 then choice=moves[2]end
   elseif g.step==2 then choice=moves[1]
   else choice=moves[1]end
   local trial=U.copy(g);assert(M.apply(trial,choice));assert(M.valid(trial));if wanted=='deck'and trial.phase==3 then local alternate=choice==moves[1]and moves[2]or moves[1];if not alternate then break end;trial=U.copy(g);assert(M.apply(trial,alternate));choice=alternate;if trial.phase==3 then break end end
   if wanted=='mermaid'and trial.phase==3 and M.count(trial,1,9,true)==4 and g.turn==1 then return g,choice end
   if wanted=='deck'and trial.phase==2 and trial.ender==0 and g.turn==1 then return g,choice end
   g=trial
  end
 end;error('no natural end fixture '..wanted)
end
for _,kind in ipairs({'mermaid','deck'})do
 local before,move=prepare(kind);local S=dofile('storage.lua');S.load(M.decode);assert(S.save(M.encode(before)));app=dofile('main.lua');on_init();on_draw();tap('continue')
 local index;for i,m in ipairs(M.moves(app.state(),nil,true))do if m[1]==move[1]and m[2]==move[2]and m[3]==move[3]then index=i;break end end;tap('act',assert(index));capture('ss-'..kind..'.pgm');dofile('export_round.lua')(app,'ss-'..kind);assert(M.valid(app.state()));assert(smudge.write_file('ss-'..kind..'.txt',M.encode(app.state()),false));on_exit();app=nil;collectgarbage('collect')
end
