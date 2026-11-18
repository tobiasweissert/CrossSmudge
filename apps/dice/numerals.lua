-- Real bundled Lato OFL glyph masks, byte-identical to candidate 2048 fonts.lua.
-- Magnify that actual artwork 2x through the native sprite ABI; no test font/UI.
local U=dofile('unified_ui.lua')
local font=dofile('numeral_fonts.lua').b52
local cache={}
local function double(g)
 local stride=(g[1]+7)//8
 local outstride=(g[1]*2+7)//8
 local rows={}
 for y=0,g[2]-1 do
  local row={}
  for j=1,stride do
   local byte=g[6]:byte(y*stride+j)
   local value=0
   for bit=7,0,-1 do value=(value<<2)|(((byte>>bit)&1)*3)end
   row[#row+1]=string.char((value>>8)&255,value&255)
  end
  local s=table.concat(row):sub(1,outstride)
  rows[#rows+1]=s;rows[#rows+1]=s
 end
 return table.concat(rows)
end
return function(cx,cy,value,maxwidth)
 local text=tostring(value)
 local width=0
 for i=1,#text do local g=font.glyphs[text:byte(i)];if not g then width=maxwidth+1;break end;width=width+g[5]*2 end
 if not text:match('^-?%d+$') or width>maxwidth then
  -- Whole long signed values remain visible, rather than being ellipsized.
  U.text(cx,cy-smudge.line_height('ui10')//2,text,'ui10',true,'center');return
 end
 local x=cx-width//2
 local cap=font.glyphs[48]
 local baseline=math.floor(cy-(cap[4]+cap[2]/2)*2)
 for i=1,#text do
  local code=text:byte(i);local g=font.glyphs[code]
  if not cache[code]then cache[code]=double(g)end
  smudge.draw_sprite(x+g[3]*2,baseline+g[4]*2,g[1]*2,g[2]*2,cache[code],false,false)
  x=x+g[5]*2
 end
end
