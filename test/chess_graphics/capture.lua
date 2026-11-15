-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- SVG native adapter: actual production font/piece bitmap pixels; legacy text uses SVG font.
local width,height=smudge.get_bounds()
local originalWrite=smudge.write_file;local current='board-start.svg'
local write=function(path,data,append)return originalWrite(path=='capture.svg'and current or path,data,append)end
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
smudge.rounded_rect=function(x,y,w,h,r,fill,c,stroke)
 emit(string.format('<rect x="%d" y="%d" width="%d" height="%d" rx="%d" fill="%s" stroke="%s" stroke-width="%d"/>',x,y,w,h,r,fill and color(c)or'none',color(c),stroke or 1))
end
smudge.bitmap=function(x,y,w,h,data,white)
 local paths={};local stride=(w+7)//8
 for row=0,h-1 do local start
  for col=0,w do local ink=col<w and data:byte(row*stride+col//8+1)&(128>>(col%8))==0
   if ink and not start then start=col elseif not ink and start then paths[#paths+1]=string.format('M%d %dh%dv1h-%dz',x+start,y+row,col-start,col-start);start=nil end
  end
 end
 emit('<path fill="'..(white and'white'or'black')..'" d="'..table.concat(paths)..'"/>')
end
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
local app=dofile('main.lua');on_init();on_draw();emit('</svg>');local G,C=app.state(),app.core;G.mode=3
local function begin(name)
 current=name;assert(write('capture.svg','<svg xmlns="http://www.w3.org/2000/svg" width="'..width..'" height="'..height..'" viewBox="0 0 '..width..' '..height..'"><desc>Native replay of actual Lua masks, not a device screenshot.</desc>',false))
end
local function tap(s)
 local l=app.layout();on_touch(l.x+(s%8)*l.size+l.size//2,l.y+(7-s//8)*l.size+l.size//2,'tap')
end
for _,pair in ipairs({{12,28},{51,35},{28,35},{59,35}})do tap(pair[1]);tap(pair[2])end
begin('board-play.svg');on_draw();emit('</svg>')
on_button('back',true);begin('settings.svg');on_draw();emit('</svg>')
G.menu=false;G.position=assert(C.parseFen('7k/P7/8/8/8/8/7p/K7 w - - 0 1'));G.moves=C.legal(G.position);G.history={};G.selected=nil;G.over=nil;G.last=nil;G.job=nil;tap(48);tap(56)
begin('promotion.svg');on_draw();emit('</svg>');on_exit();print('Original font/piece captures: initial, captured pieces/SAN sheet, settings, promotion')
