-- Run APP2048_SOURCE=test/2048_design/baseline.lua for the RED reference check.
local H=smudge.dofile('test/2048_design/support.lua')
local app=H.app(os.getenv('APP2048_SOURCE') or 'main.lua','sticky',{})
local header,sprite=false,false
app.smudge.header=function() header=true end
local draw=app.smudge.draw_sprite
app.smudge.draw_sprite=function(...) sprite=true;return draw(...) end
app.on_draw()
assert(not header and sprite,'reference contract: large bitmap typography, no legacy header')
print('PASS: reference typography contract')
