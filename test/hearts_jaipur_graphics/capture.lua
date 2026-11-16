-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- Actual application on_draw calls, recorded as SVG by a native test adapter.
-- Coordinates and monochrome geometry are retained; font metrics and header
-- chrome are approximate. This is not a hardware/firmware screenshot.
local width,height=smudge.get_bounds()
local started=false
local function esc(s)return tostring(s):gsub('&','&amp;'):gsub('<','&lt;'):gsub('>','&gt;'):gsub('"','&quot;') end
local function add(s)assert(smudge.write_file('render.svg',s,true))end
smudge.clear=function()
 assert(smudge.write_file('render.svg','<svg xmlns="http://www.w3.org/2000/svg" width="'..width..'" height="'..height..'" viewBox="0 0 '..width..' '..height..'"><desc>Actual Lua draw calls; native capture, not a device screenshot.</desc><rect width="100%" height="100%" fill="white"/>',false))
end
smudge.rounded_rect=function(x,y,w,h,r,fill,color,stroke)
 add(string.format('<rect x="%d" y="%d" width="%d" height="%d" rx="%d" fill="%s" stroke="%s" stroke-width="%d"/>',x,y,w,h,r,fill and (color==false and 'white' or 'black') or 'none',color==false and 'white' or 'black',stroke or 1))
end
smudge.bitmap=function(x,y,w,h,data,white,gray)
 local paths={};local stride=(w+7)//8
 for row=0,h-1 do local start
  for col=0,w do
   local ink=col<w and (data:byte(row*stride+col//8+1)&(128>>(col%8)))==0
   if ink and not start then start=col elseif not ink and start then paths[#paths+1]=string.format('M%d %dh%dv1h-%dz',x+start,y+row,col-start,col-start);start=nil end
  end
 end
 add('<path fill="'..(white and 'white' or gray and '#888' or 'black')..'" d="'..table.concat(paths)..'"/>')
end
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
local M=dofile('core.lua');local hearts=M.title=='Hearts';width,height=hearts and 800 or 480,hearts and 480 or 800
smudge.get_bounds=function()return width,height end;smudge.get_orientation=function()return 'portrait'end;smudge.set_orientation=function()end
local write=smudge.write_file;local current='menu.svg'
smudge.write_file=function(path,data,append)return write(path=='render.svg'and current or path,data,append)end
local app=dofile('main.lua');on_init();on_draw();add('</svg>');app.view.menu=false;current='board.svg';on_draw();add('</svg>')
for page=1,hearts and 4 or 8 do app.view.help=page;current='help-'..page..'.svg';on_draw();add('</svg>')end;app.view.help=0
local g=app.state()
for i=1,4000 do
 if hearts then
  if g.phase>=3 then break elseif g.phase==0 then M.commit(g,{M.passpick(g,1),M.passpick(g,2),M.passpick(g,3),M.passpick(g,4)})elseif g.phase==1 then M.play(g,M.choose(g))else M.sweep(g)end
 else if g.phase>0 then break else M.apply(g,M.ai(g))end end
end
current='scores.svg';on_draw();add('</svg>');on_exit();print('Native original-mask menu, board, help and scores captures')
