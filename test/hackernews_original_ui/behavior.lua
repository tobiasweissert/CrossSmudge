T.open();T.update();T.complete('[11]');T.update()
local title=string.rep('Synthetic title word ',30)
local read=smudge.read_file;local font=assert(read('art/reading_serif_14.bin',32768))
local i=(46-32)*8+1;local w,h,_,_,_,_,lo,hi=font:byte(i,i+7);local offset=lo+hi*256;local dot=font:sub(offset+1,offset+((w+7)//8)*h)
local bitmap=smudge.bitmap;local dots=0
smudge.bitmap=function(x,y,gw,gh,data,...)if gw==w and gh==h and data==dot then dots=dots+1 end;return bitmap(x,y,gw,gh,data,...)end
T.complete(T.story(11,title,7));dots=0;on_draw()
assert(dots>=3,'long source-shaped headline silently drops its tail without ellipsis')
-- A view wrapper must not revive drawing/navigation after the controller exits.
T.tap('feeds');local view=T.view();local focus=view.menuFocus;on_exit();on_button('down',true)
assert(view.menuFocus==focus,'presentation handles input after controller exit')
print('PASS bounded presentation cases')
-- Every item in the immutable six-row controller page remains reachable when
-- the original92px rows only fit two at landscape size.
T.reset();T.update();T.complete('[11,12,13,14,15,16,17,18]')
for id=11,16 do T.update();T.complete(T.story(id,'Story '..id,id))end
local seen={};local lastPage=T.state().listPage
for step=1,3 do
 on_draw();for _,b in ipairs(T.view().hit)do if b.kind=='story'then seen[T.state().items[b.id].id]=true end end
 if T.view().slices==1 or step==3 then break end
 T.button('page_forward')
end
for id=11,16 do assert(seen[id],'original row layout hides controller item '..id)end
if T.view().slice>0 then T.button('confirm');assert(T.state().selected.id==15,'physical confirm opened a hidden first row');T.button('back')end
-- Move across the real controller page boundary; deferred item loads remain
-- controller-owned and the local view resets at the matching boundary.
while T.state().listPage==lastPage do T.button('page_forward')end
for id=17,18 do T.update();T.complete(T.story(id,'Story '..id,id))end
assert(T.state().items[1].id==17 and T.state().items[2].id==18)
T.shot('list-last-controller-page')
T.tap('feeds');local focus=T.view().menuFocus
local rect=smudge.rect;local markers=0
smudge.rect=function(x,y,w,h,filled,stroke,...)if not filled and stroke==3 then markers=markers+1 end;return rect(x,y,w,h,filled,stroke,...)end
T.button('down');assert(T.view().menuFocus~=focus and markers>0,'physical feed focus is invisible')
-- Four current feeds remain real destinations, not native Saved placeholders.
for tab,name in ipairs({'topstories','newstories','askstories','showstories'})do
 if not T.view().feedMenu then T.tap('feeds')end
 T.tap('tab',tab);T.update();assert(T.state().tab==tab and T.requests()[#T.requests()].url:find(name..'.json',1,true))
 T.complete('[99]');T.update();T.complete(T.story(99,'Feed '..tab,tab))
end
-- Direct article/comment operations use drawn objects and the unchanged
-- controller paths (including cancellation and deferred cursor ownership).
T.tap('story',1);T.update();T.complete('First paragraph.\nSecond paragraph.')
assert(T.state().screen=='article'and T.state().articleText=='First paragraph.\nSecond paragraph.')
T.shot('article');T.tap('comments');assert(T.state().screen=='comments');on_draw();T.shot('empty-thread');assert(table.concat(T.view().visibleLines):find('No comments on this one yet.',1,true),'empty thread omits original source explanation')
T.tap('article');assert(T.state().screen=='article');T.button('back');assert(T.state().screen=='comments')
T.button('back');assert(T.state().screen=='list')
print('PASS six-row visibility/physical selection, all4feeds, articles, comments and Back')
