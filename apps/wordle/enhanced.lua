-- Sticky presentation adapter; the original controller owns rules and saves.
local app=dofile('cp_app.lua')
local V=dofile('board.lua')
local oldInit,oldTouch,oldButton=on_init,on_touch,on_button
local oldLayout=app.layout
local state=app.state()
local layout
local function action(id)
 local l=oldLayout()
 oldTouch(12+(id-1)%10*l.kw+2,l.ky+(id-1)//10*(l.kh+4)+2,'tap')
end
function on_init()
 oldInit();V.init();local w,h=smudge.get_bounds();layout=V.layout(w,h)
end
function on_draw()
 if state.closed then return end
 local w,h=smudge.get_bounds();layout=V.layout(w,h)
 V.draw(layout,state.game,state.notice,state,action)
 if state.menu then V.drawMenu(layout,state) end
end
function on_touch(x,y,event)
 if state.closed or event~='tap' then return end
 if state.menu then
  for n,b in ipairs(layout.menu or {}) do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then state.menuIndex=n;oldButton('confirm',true);return end end
 else V.touch(layout,x,y,false,action,function()action(30)end) end
end
app.layout=function()return layout end
return app
