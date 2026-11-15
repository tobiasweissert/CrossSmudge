-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- SVG native adapter: production drawing calls/piece bitmap pixels; substitute SVG font.
local width,height=smudge.get_bounds()
local write=smudge.write_file
assert(write('capture.svg','<svg xmlns="http://www.w3.org/2000/svg" width="'..width..'" height="'..height..'" viewBox="0 0 '..width..' '..height..'"><title>Chess production drawing adapter; SVG font, not device renderer</title>',false))
local function emit(s) assert(write('capture.svg',s,true)) end
local function esc(s) return tostring(s):gsub('&','&amp;'):gsub('<','&lt;'):gsub('>','&gt;'):gsub('"','&quot;') end
local function color(b) return b==false and 'white' or 'black' end
smudge.clear=function() emit('<rect width="100%" height="100%" fill="white"/>') end
smudge.rect=function(x,y,w,h,fill,a,b)
 local c=type(a)=='boolean' and a or b;local stroke=type(a)=='number' and a or 1
 emit('<rect x="'..x..'" y="'..y..'" width="'..w..'" height="'..h..'" fill="'..(fill and color(c) or 'none')..'" stroke="'..color(c)..'" stroke-width="'..stroke..'"/>')
end
smudge.rect_dither=function(x,y,w,h) emit('<rect x="'..x..'" y="'..y..'" width="'..w..'" height="'..h..'" fill="#ccc"/>') end
smudge.text=function(x,y,s,font,bold,align,c)
 emit('<text x="'..x..'" y="'..(y+15)..'" font-family="monospace" font-size="14" font-weight="'..(bold and 'bold' or 'normal')..'" text-anchor="'..(align=='center' and 'middle' or align=='right' and 'end' or 'start')..'" fill="'..color(c)..'">'..esc(s)..'</text>')
end
smudge.header=function(title,sub) smudge.text(12,10,title,10,true);smudge.text(12,31,sub or '',10,false) end
smudge.circle=function(x,y,r,fill,c) emit('<circle cx="'..x..'" cy="'..y..'" r="'..r..'" fill="'..(fill and color(c) or 'none')..'" stroke="'..color(c)..'"/>') end
smudge.draw_sprite=function(x,y,w,h,path,inverted)
 local data=assert(smudge.read_file(path,4096));local stride=(w+7)//8
 assert(#data>=stride*h)
 for yy=0,h-1 do for xx=0,w-1 do if data:byte(yy*stride+xx//8+1)&(1<<(7-xx%8))==0 then
  emit('<rect x="'..(x+xx)..'" y="'..(y+yy)..'" width="1" height="1" fill="'..(inverted and 'white' or 'black')..'"/>')
 end end end
end
smudge.button_hints=function(...)
 for i,s in ipairs({...}) do local x=(i-1)*width//4;smudge.rect(x,height-40,width//4,40,false,1,true);smudge.text(x+width//8,height-30,s,10,false,'center') end
end
local app=dofile('main.lua');on_init();on_draw();emit('</svg>');on_exit()
print('production draw SVG captured with substitute text font')
