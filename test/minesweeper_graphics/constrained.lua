-- Actual entry point without the larger synthetic-board fixture's retained data.
assert(math.maxinteger==2147483647)
dofile('main.lua');on_init();on_button('confirm',true);on_draw();capture('covered.pgm')
on_long_press(30,110);on_draw();capture('flag.pgm')
on_long_press(30,110);on_touch(30,110,'tap');on_draw();capture('opened.pgm')
on_button('back',true);on_button('down',true);on_button('down',true);on_button('down',true);on_button('confirm',true)
for i=1,5 do on_draw();capture('help'..i..'.pgm');on_button('confirm',true) end
on_exit()
