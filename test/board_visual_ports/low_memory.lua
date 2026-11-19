assert(smudge.get_memory().lua_max_kb<512)
dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();on_exit()
print('Constrained legacy dispatch passed')
