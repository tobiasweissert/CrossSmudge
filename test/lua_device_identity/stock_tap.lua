-- Identity is supplied by compiled LuaRunner::l_getDevice, never spoofed here.
local app=dofile('main.lua')
assert(type(app)=='table' and app.state, 'production identity selected legacy Solitaire')
on_init();on_draw()
-- The full-memory graphical app starts on its existing home menu; use the
-- normal Confirm action before inspecting the playing board's stock target.
if app.layout().menu then on_button('confirm',true);on_draw() end
local g=app.state();local l=app.layout()
local stock,waste,moves=#g.piles[0],#g.piles[1],g.moves
assert(stock>0)
assert(smudge.test_touch(l.x+l.cw//2,l.top+2,'tap'));on_draw()
assert(#g.piles[0]<stock and #g.piles[1]>waste and g.moves>moves,
       'stock tap must deal cards without physical keys')
on_exit()
print('production identity -> real Solitaire -> stock tap passed')
