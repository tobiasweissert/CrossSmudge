local a=T.open();assert(a.state().home and not a.state().job);T.shot('front')
local original=a.core.fen(a.state().position);T.tap(a.layout().start[2]);assert(a.state().nearbyUnavailable and a.core.fen(a.state().position)==original);T.shot('nearby-unavailable');T.button('back');assert(a.state().home)
T.tap(a.layout().start[3]);assert(a.state().menu and a.state().menuFromHome);T.shot('settings-menu');T.setting(1);assert(a.state().home)
T.tap(a.layout().start[1]);assert(not a.state().home);T.shot('board');T.square(12);assert(a.state().selected==12);T.shot('selected');T.square(28);assert(a.state().job)
T.tap(a.layout().gear);assert(a.state().menu and not a.state().job);T.shot('settings-board');T.setting(1);assert(a.state().job)
while a.state().job do on_update()end;on_draw();assert(#a.state().history==2);T.shot('move-list')
T.setting(5);assert(#a.state().history==0);T.setting(3);assert(a.state().restartPending and a.state().mode==1,'opponent preference mutated active game before confirmation');T.shot('settings-restart');T.setting(1);assert(a.state().mode==3 and #a.state().history==0)
on_update();local checkpoint=a.core.fen(a.state().position)
T.fail(true);T.square(13);local l=a.layout();on_touch(l.x+5*l.size+l.size//2,l.y+5*l.size+l.size//2,'tap');assert(#a.state().history==1)
-- The second queued contact was painted for the other board orientation.
on_touch(l.x+l.size//2,l.y+l.size//2,'tap');assert(a.state().selected==nil and #a.state().history==1,'stale orientation contact reached a new position');on_draw();on_update();assert(a.state().notice:find('Save failed'));T.shot('save-failure');on_exit();T.fail(false)
a=T.open();assert(a.core.fen(a.state().position)==checkpoint,'failed save replaced previous durable board');T.button('confirm')
for _,m in ipairs({{13,21},{52,36},{14,30},{59,31}})do T.square(m[1]);T.square(m[2])end
assert(a.state().over=='Checkmate');T.shot('checkmate');T.button('back');assert(a.state().home);T.shot('saved-front');on_exit()
a=T.seed('7k/P7/8/8/8/8/7p/K7 w - - 0 1');T.button('confirm');T.square(48);T.square(56);assert(#a.state().promotion==4);T.shot('promotion');T.tap(a.layout().buttons[4]);assert(a.state().position.squares[56]==2);on_update();on_exit()
a=T.open();assert(a.state().position.squares[56]==2 and a.state().home);T.button('confirm');T.shot('underpromotion-reopen');on_exit()

a=T.open();T.button('confirm');T.setting(9);T.setting(1);T.square(15);assert(a.hints()==false and a.state().selected==15);T.shot('hints-off');on_exit()

a=T.open();T.button('confirm');T.setting(3);T.setting(8);assert(a.state().uiMode==2);T.setting(1);assert(a.state().mode==2 and a.state().job);T.shot('black-thinking');on_exit()
a=T.seed('7k/8/8/8/8/8/8/K7 w - - 100 65535',3);T.button('confirm');assert(a.state().over=='Draw: fifty moves');T.shot('fifty-move');on_exit()
a=T.seed('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',3);T.button('confirm')
for _=1,2 do for _,m in ipairs({{1,18},{57,42},{18,1},{42,57}})do T.square(m[1]);T.square(m[2])end end
assert(a.state().over=='Draw: threefold repetition');T.shot('repetition');on_exit()
a=T.seed('7k/8/8/8/8/8/8/K7 w - - 0 65534',3);T.button('confirm');T.square(0);T.square(8);T.square(63);T.square(55);assert(#a.state().history==2);T.shot('long-move-number');on_exit()
a=T.seed('7k/5Q2/6K1/8/8/8/8/8 b - - 0 1',3);T.button('confirm');assert(a.state().over=='Stalemate');T.shot('stalemate');on_exit()
for index,kind in ipairs({5,4,3,2})do
 a=T.seed('7k/P7/8/8/8/8/7p/K7 w - - 0 1',3);T.button('confirm');T.square(48);T.square(56);T.tap(a.layout().buttons[index]);assert(a.state().position.squares[56]==kind,'promotion touch chose wrong piece');on_update();T.shot('promotion-result-'..index);on_exit()
end
