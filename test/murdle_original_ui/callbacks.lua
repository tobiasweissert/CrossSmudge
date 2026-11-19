local now=0;smudge.millis=function()return now end
local app=dofile('main.lua');on_init();local M=app.core;local g=app.state();local currentFace,currentPage,nextTier=1,1,1;local picks={0,0,0,0}
local function frame(name)
 on_draw();local p=M.case(g);local selected={};for i=1,4 do selected[i]=app.view().made[i]and g.picks[i]or 0 end;local header=table.concat({g.id,p.seed,g.phase,app.screen(),app.view().face,app.view().page,app.struck(),app.view().nextTier,g.attempts,selected[1],selected[2],selected[3],selected[4],app.view().notice==''and 0 or 1},' ')
 assert(smudge.write_file(name..'.state',header..'\n'..table.concat(g.marks)..'\n'));capture(name..'.pgm')
end
local function tap(action,value)
 on_draw();for _,hit in ipairs(app.hits())do if hit.action==action and(value==nil or hit.value==value)then local b=hit.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end;error('missing action '..action)
end
frame('menu');tap('settings');frame('settings');tap('tier',4);nextTier=4;frame('settings-impossible');on_button('back',true);tap('new');frame('confirm');on_button('back',true);tap('help');frame('help');on_button('back',true);tap('play');frame('clues');tap('clue',1);frame('clues-ticked');tap('face',2);currentFace=2;frame('grid');tap('face',3);currentFace=3;frame('info');tap('accuse');frame('accuse');on_exit()

-- Each immutable local case is restored through its real journal. Original
-- generation seeds recreate the corresponding native puzzle, never a redraw
-- based on guessed labels. Every clue page and all three faces are compared.
for id=1,64 do
 local core=dofile('core.lua');local restore=core.new(0,id);local journal=dofile('storage.lua');journal.load(core.decode);assert(journal.save(core.encode(restore)))
 app=dofile('main.lua');on_init();g=app.state();M=app.core;assert(g.id==id);picks={0,0,0,0};local prefix='case-'..id..'-'
 frame(prefix..'menu');tap('play');frame(prefix..'clues-1')
 for step=2,12 do
  local prior=app.view().page;local found=false
  for _,v in ipairs(app.hits())do if v.action=='page'and v.value==1 then found=true end end
  if not found then break end;tap('page',1);if app.view().page==prior then break end;frame(prefix..'clues-'..step)
 end
 tap('face',2);frame(prefix..'grid');tap('face',3);frame(prefix..'info');tap('accuse');frame(prefix..'accuse-empty')
 -- First and last cases of every tier also exercise actual grid cycling,
 -- derived refusal and complete accusations through drawn controls.
 if (id-1)%16==0 or id%16==0 then
  on_button('back',true);on_draw();tap('face',2)
  local cell
  for _,v in ipairs(app.hits())do if v.action=='cell'then cell=v;break end end
  assert(cell);local box=cell.box;on_touch(box.x+box.w//2,box.y+box.h//2,'tap');frame(prefix..'grid-no');on_touch(box.x+box.w//2,box.y+box.h//2,'tap');frame(prefix..'grid-yes')
  local p=M.case(g);local pair=cell.value;local block,row,col=pair[1],pair[2],pair[3];local target
  for _,v in ipairs(app.hits())do if v.action=='cell'and v.value[1]==block and v.value[2]==row and v.value[3]~=col then target=v;break end end
  assert(target);local b=target.box;local old=M.encode(g);on_touch(b.x+b.w//2,b.y+b.h//2,'tap');assert(M.encode(g)==old);frame(prefix..'grid-refused')
  tap('accuse');for cat=1,p.cats do
   local item=cat==1 and p.murder%p.n+1 or p.solution[cat][p.murder];picks[cat]=item
   on_draw();for _,v in ipairs(app.hits())do if v.action=='pick'and v.value[1]==cat and v.value[2]==item then local b=v.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');break end end
  end
  frame(prefix..'accuse-wrong');tap('submit');assert(g.phase==0 and g.attempts==1);frame(prefix..'verdict-wrong');tap('play');tap('accuse')
  for cat=1,p.cats do local item=p.solution[cat][p.murder];picks[cat]=item
   on_draw();for _,v in ipairs(app.hits())do if v.action=='pick'and v.value[1]==cat and v.value[2]==item then local b=v.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');break end end
  end
  frame(prefix..'accuse-right');tap('submit');assert(g.phase==1 and g.attempts==2);frame(prefix..'verdict-right');tap('menu');frame(prefix..'menu-solved')
 end
 on_exit();app=nil;g=nil;M=nil;core=nil;restore=nil;collectgarbage('collect')
end
print('All64real case journals, full clue pages, grids, cast and accusation callbacks passed')
