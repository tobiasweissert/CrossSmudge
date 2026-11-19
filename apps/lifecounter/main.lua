-- Life Counter: unrestricted signed total; legacy save and reset semantics.
local U=dofile('unified_ui.lua')
local numeral=dofile('numerals.lua')
local lifeTotal=20
local footer={}
local w,h=480,800
function on_init()w,h=smudge.get_bounds();lifeTotal=tonumber(smudge.load('life','20')) or 20 end
local function save_state()smudge.save('life',tostring(lifeTotal))end
function on_draw()
 smudge.clear();local m=U.metrics();w,h=m.w,m.h
 U.header('Life Counter')
 local cy=(m.body_top+m.body_bottom)//2
 local radius=math.min(165,(m.body_bottom-m.body_top)//2-20)
 smudge.heart(w//2,cy,radius,5,true)
 U.text(14,cy-21,'-5','lexend16',true)
 U.text(w-14,cy-21,'+5','lexend16',true,'right')
 numeral(w//2,cy,lifeTotal,radius*2-36)
 footer=U.footer({'Back','Reset','-1','+1'},4)
end
function on_button(btn,pressed)
 if not pressed then return end
 if btn=='back' then save_state();smudge.exit();return
 elseif btn=='confirm' then lifeTotal=20
 elseif btn=='up' then lifeTotal=lifeTotal-5
 elseif btn=='down' then lifeTotal=lifeTotal+5
 elseif btn=='left' then lifeTotal=lifeTotal-1
 elseif btn=='right' then lifeTotal=lifeTotal+1
 else return end
 save_state()
end
function on_tap(x,y)
 local cw,ch=smudge.get_bounds();if cw~=w or ch~=h then return end
 for i,r in ipairs(footer)do if U.hit(r,x,y)then on_button(({'back','confirm','left','right'})[i],true);return end end
 local m=U.metrics();if x<0 or x>=m.w or y<m.body_top or y>=m.body_bottom then return end
 if x<60 then on_button('up',true)
 elseif x>w-60 then on_button('down',true)
 elseif x<w//2 then on_button('left',true)
 else on_button('right',true)end
end
