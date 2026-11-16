local M=dofile('core.lua'); local game=M.new(123); local original=M.new
M.new=function()return game end
local glyphs,dither=0,0
smudge.bitmap=function(...)glyphs=glyphs+1 end
smudge.rect_dither=function(...)dither=dither+1 end
smudge.get_device=function()return 'sticky' end
-- No native/generic text is allowed in the board render.
smudge.header=function()error('generic header')end
smudge.text=function()error('generic text')end
smudge.button_hints=function()error('generic hints')end
collectgarbage('collect');dofile('ui.lua').install(M);on_init();on_button('confirm',true);on_draw()
assert(dither==80 and glyphs>0,'actual graphical tiles and shipped glyphs required')
assert(type(on_long_press)=='function','real opt-in callback required')
on_long_press(30,110);assert(game.b[1]==4 and game.status==0)
on_long_press(30,110);assert(game.b[1]==0 and game.status==0)
on_touch(30,110,'tap');assert(game.b[1]&2~=0 and game.status==1)
local before=M.encode(game);on_long_press(30,110);assert(M.encode(game)==before)
on_long_press(-1,110);on_long_press(480,110);assert(M.encode(game)==before)
print('graphics, flag-only hold, first safe dig, revealed/outside hold PASS')
