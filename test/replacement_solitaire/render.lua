-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- Actual application on_draw calls, recorded as SVG by a native test adapter.
-- Coordinates and monochrome geometry are retained; font metrics and header
-- chrome are approximate. This is not a hardware/firmware screenshot.
local width,height=smudge.get_bounds()
local nodes={}
local function esc(s)return tostring(s):gsub('&','&amp;'):gsub('<','&lt;'):gsub('>','&gt;'):gsub('"','&quot;') end
local function add(s)nodes[#nodes+1]=s end
smudge.clear=function()nodes={} end
smudge.rect=function(x,y,w,h,fill,a,b)
 local color,stroke=true,1
 if fill then if a~=nil then color=a~=false and a~=0 end
 elseif type(a)=='boolean' then color=a;stroke=b or 1
 else stroke=a or 1;if b~=nil then color=b~=false and b~=0 end end
 add(string.format('<rect x="%d" y="%d" width="%d" height="%d" fill="%s" stroke="%s" stroke-width="%d"/>',x,y,w,h,fill and (color and 'black' or 'white') or 'none',fill and 'none' or (color and 'black' or 'white'),stroke))
end
smudge.line=function(x1,y1,x2,y2,color)add(string.format('<path d="M %d %d L %d %d" fill="none" stroke="%s"/>',x1,y1,x2,y2,color==false and 'white' or 'black')) end
smudge.text=function(x,y,text,font,bold,align,color)
 local size=(font or 10)*1.4
 add(string.format('<text x="%d" y="%d" fill="%s" font-family="sans-serif" font-size="%.1f" font-weight="%s" dominant-baseline="hanging" text-anchor="%s">%s</text>',x,y,color==false and 'white' or 'black',size,bold and 'bold' or 'normal',align=='center' and 'middle' or align=='right' and 'end' or 'start',esc(text)))
end
smudge.header=function(title,subtitle)smudge.text(18,12,title,18,true);if subtitle then smudge.text(width-18,22,subtitle,10,false,'right') end;smudge.line(12,58,width-12,58) end
smudge.button_hints=function(a,b,c,d)local labels={a,b,c,d};for i=1,4 do smudge.text((i-0.5)*width//4,height-28,labels[i] or '',10,false,'center') end end
smudge.circle=function(x,y,r,fill,color)add(string.format('<circle cx="%d" cy="%d" r="%d" fill="%s" stroke="black"/>',x,y,r,fill and (color==false and 'white' or 'black') or 'none')) end
smudge.rect_dither=function(x,y,w,h,dark)add(string.format('<rect x="%d" y="%d" width="%d" height="%d" fill="%s"/>',x,y,w,h,dark and '#777' or '#ddd')) end
smudge.get_date=function()return {year=2026,month=10,day=2} end
local app=dofile('main.lua');on_init();on_draw()
local svg='<svg xmlns="http://www.w3.org/2000/svg" width="'..width..'" height="'..height..'" viewBox="0 0 '..width..' '..height..'"><desc>Native adapter rendering of actual Lua on_draw. Approximate fonts and chrome; not on-device evidence.</desc><rect width="100%" height="100%" fill="white"/>'..table.concat(nodes)..'</svg>'
assert(smudge.write_file('render.svg',svg,false));on_exit()
