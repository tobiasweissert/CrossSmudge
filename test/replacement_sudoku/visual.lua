-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- SVG adapter for production on_draw geometry. Font metrics and header/hint
-- rendering are approximations, not framebuffer captures or hardware evidence.
local write = smudge.write_file
local width,height=smudge.get_bounds()
local parts={}
local function emit(s) parts[#parts+1]=s end
local function esc(s) return tostring(s):gsub('&','&amp;'):gsub('<','&lt;'):gsub('>','&gt;'):gsub('"','&quot;') end
smudge.read_file=function() return nil end
smudge.write_file=function() return false end
smudge.clear=function()
  parts={}
  emit('<svg xmlns="http://www.w3.org/2000/svg" width="'..width..'" height="'..height..'" viewBox="0 0 '..width..' '..height..'">')
  emit('<title>Sudoku production draw calls, native SVG adapter; approximate fonts</title>')
  emit('<rect width="100%" height="100%" fill="white"/>')
end
smudge.rect=function(x,y,w,h,fill,stroke,color)
  if type(stroke)=='boolean' then color,stroke=stroke,1 end
  local ink=color==false and 'white' or 'black'
  emit(string.format('<rect x="%d" y="%d" width="%d" height="%d" fill="%s" stroke="%s" stroke-width="%d"/>',x,y,w,h,fill and ink or 'none',ink,stroke or 1))
end
smudge.line=function(x,y,x2,y2,color)
  emit(string.format('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s"/>',x,y,x2,y2,color==false and 'white' or 'black'))
end
smudge.text=function(x,y,text,font,bold,align,color)
  emit(string.format('<text x="%d" y="%d" font-family="sans-serif" font-size="16" font-weight="%s" text-anchor="%s" fill="%s">%s</text>',x,y+16,bold and 'bold' or 'normal',align=='center' and 'middle' or align=='right' and 'end' or 'start',color==false and 'white' or 'black',esc(text)))
end
smudge.header=function(title,sub)
  smudge.text(12,12,title,10,true,'left',true)
  smudge.text(width-12,12,sub or '',10,false,'right',true)
  smudge.line(0,48,width,48,true)
end
smudge.button_hints=function(a,b,c,d)
  local labels={a,b,c,d}
  smudge.line(0,height-40,width,height-40,true)
  for i,label in ipairs(labels) do smudge.text((2*i-1)*width//8,height-30,label,10,false,'center',true) end
end
local app=dofile('main.lua')
on_init();on_draw();on_button('right',true);on_button('confirm',true)
for i=1,10000 do if not app.state().job then break end;on_update() end
assert(app.state().given,'visual fixture could not generate')
on_draw()
emit('</svg>')
assert(write('adapter.svg',table.concat(parts,'\n'),false))
on_exit()
print('production draw geometry captured with approximate adapter fonts')
