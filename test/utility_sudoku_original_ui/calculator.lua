local a=T.open();T.shot('calculator')
local function key(n)T.tap(a.cells()[n])end
key(5);key(8);T.shot('calculator-pending');key(9);key(20);T.shot('calculator-result');assert(a.core.display(a.state())=='28')
key(1);key(16);key(4);key(18);key(20);T.shot('calculator-error')
on_long_press(100,100);on_draw();T.shot('calculator-menu');T.button('back')
T.fail(true);T.button('back');assert(not T.exited(),'failed save closed calculator');T.shot('calculator-save-failure');T.fail(false);T.button('back');assert(T.exited(),'successful retry did not close');on_exit()
a=T.open();key(1);for _=1,10 do key(7)end;key(8);for _=1,10 do key(7)end;key(20);assert(#a.core.display(a.state())<=16);T.shot('calculator-long');on_exit()

a=T.open();on_long_press(100,100);on_draw();on_touch(100,350,'tap');on_draw();assert(a.core.display(a.state())=='0');on_long_press(100,100);on_draw();on_touch(100,160,'tap');assert(T.exited());on_exit()
