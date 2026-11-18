-- LuaRunner essential libraries intentionally omit the optional utf8 library.
utf8=utf8 or dofile('utf8_compat.lua')
local app=dofile('reader.lua')
-- Presentation-only viewport slices; every accepted controller callback stays
-- unchanged and receives the actual drawn row via its existing touch path.
if app.view.beforeButton then
 local button,swipe,exit,draw=on_button,on_swipe,on_exit,on_draw
 local closed=false
 function on_exit()closed=true;app.view.retire();exit()end
 function on_draw()if not closed then draw();app.view.complete(app.state())end end
 function on_button(name,pressed)
  if closed then return end
  if pressed~=false and name~='back'and not app.view.valid(app.state())then return end
  if pressed~=false and app.view.beforeButton(name)then return end
  button(name,pressed)
 end
 function on_swipe(direction)
  if closed or not app.view.valid(app.state())then return end
  if app.view.navigate((direction=='up'or direction=='left')and 1 or -1)then return end
  swipe(direction)
 end
end
return app
