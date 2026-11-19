assert(smudge.get_device()=='sticky');local app=dofile('main.lua');on_init();on_draw();capture('menu.pgm');on_exit()
