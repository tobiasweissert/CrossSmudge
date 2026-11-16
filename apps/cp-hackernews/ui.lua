-- Wrap UTF-8 by measured glyph strings, including long unbroken URLs. Retain
-- only the current screen's lines, not an entire article's layout.
local U={page=1,pages=1}
function U.draw(title,body,status,hints)
  local w,h=smudge.get_bounds()
  local m=smudge.get_metrics()
  local pad=m.content_side_padding or 20
  local top=m.header_height+m.top_padding
  local line=smudge.line_height('ui12')
  local footer=math.max(m.button_hints_height,smudge.has_touch() and line+16 or 0)
  local bottom=h-footer
  local capacity=math.max(1,math.floor((bottom-top)/line)-3)
  local maxwidth=w-2*pad
  local lines,total={},0
  local function emit(s)
    total=total+1
    if total>(U.page-1)*capacity and total<=U.page*capacity then lines[#lines+1]=s end
  end
  local current=''
  -- All incoming text was UTF-8 validated before reaching this function.
  for _,cp in utf8.codes(body) do
    local c=utf8.char(cp)
    if c=='\n' then emit(current); current=''
    elseif smudge.text_width(current..c,'ui12')>maxwidth then
      emit(current); current=c
    else current=current..c end
  end
  emit(current)
  U.pages=math.max(1,math.ceil(total/capacity))
  if U.page>U.pages then U.page=U.pages; return U.draw(title,body,status,hints) end
  smudge.clear(); smudge.header(title)
  for i,s in ipairs(lines) do smudge.text(pad,top+(i-1)*line,s,'ui12') end
  -- Status is bounded and wrapped using the same width rule on its own line.
  local label='['..U.page..'/'..U.pages..'] '..status
  while smudge.text_width(label,'ui12')>maxwidth do
    local at=utf8.offset(label,-1); label=label:sub(1,at-1)
  end
  smudge.text(pad,bottom-2*line,label,'ui12')
  if smudge.has_touch() then
    for i,label in ipairs({'Back',hints or 'Menu','Prev','Next'}) do
      smudge.rect((i-1)*w//4+2,bottom+2,w//4-4,footer-4,false,1,true)
      smudge.text((i-1)*w/4+8,bottom+8,label,'ui12')
    end
  else smudge.button_hints('Back',hints or 'Menu','Prev','Next') end
end
function U.turn(delta) U.page=math.max(1,math.min(U.pages,U.page+delta)); smudge.request_update() end
function U.touch(x,y,event,button)
  if event~='tap' then return end
  local w,h=smudge.get_bounds(); local m=smudge.get_metrics()
  if x<0 or x>=w or y<0 or y>=h then return end
  local footer=math.max(m.button_hints_height,smudge.has_touch() and smudge.line_height('ui12')+16 or 0)
  if y>=h-footer then
    button(({'back','confirm','up','down'})[math.floor(x*4/w)+1])
  elseif y>=m.header_height+m.top_padding then
    button(x<w/2 and 'up' or 'down')
  end
end
return U
