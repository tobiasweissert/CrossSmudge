-- Guard before loading the translated engine: measured 75 KiB C3 budget is insufficient.
local memory=smudge.get_memory()
if memory.lua_max_kb<256 then
 function on_draw()
  smudge.clear();smudge.header("Chess", "More Lua memory required")
  smudge.centered_text(110,"This CrossPlay port needs a PSRAM device.",10,true,true)
  smudge.centered_text(146,"Your previous save has not been changed.",10,false,true)
  smudge.button_hints("Back","Exit","","")
 end
 function on_button(button,pressed) if pressed and (button=="back" or button=="confirm") then smudge.exit() end end
 function on_touch(x,y,event) if event=="tap" then smudge.exit() end end
 return {unsupported=true}
end
return dofile("app.lua")
