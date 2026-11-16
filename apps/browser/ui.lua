-- App-local UI: bounded text, shared touch/button hit targets, no framebuffer.
local U={buttons={},focus=1,notice=''}
function U.redraw() smudge.request_update() end
function U.begin(title,subtitle)
 if not U.editor then U.editorDraw=nil end
 U.w,U.h=smudge.get_bounds(); U.m=smudge.get_metrics()
 U.pad=math.max(12,U.m.content_side_padding or 20)
 U.top=U.m.top_padding+U.m.header_height+12
 U.bottom=U.h-U.m.button_hints_height-12
 U.buttons={};smudge.clear();smudge.header(title,subtitle or '')
end
function U.text(x,y,s,font,bold,align,color)
 smudge.text(math.floor(x),math.floor(y),tostring(s),font or 'ui12',bold and 'bold' or 'regular',align or 'left',color~=false)
end
function U.fit(s,width,font,style)
 font=font or 'ui12'
 while #s>0 and smudge.text_width(s,font,style)>width do
  local at=#s;while at>1 and s:byte(at)>=128 and s:byte(at)<192 do at=at-1 end
  s=s:sub(1,at-1)
 end
 return s
end
function U.wrap(s,x,y,width,lines,font,bold)
 font=font or 'ui12'
 local result=smudge.wrapped_text(s,width,font,bold and 'bold' or 'regular',lines)
 for i,line in ipairs(result.lines) do U.text(x,y+(i-1)*smudge.line_height(font),line,font,bold) end
 return y+#result.lines*smudge.line_height(font)
end
function U.button(label,x,y,w,h,action,filled,value)
 local i=#U.buttons+1; U.buttons[i]={x,y,w,h,action,label,value}
 smudge.rounded_rect(x,y,w,h,math.min(14,h//4),filled or false,U.focus==i and 3 or 1)
 U.text(x+w/2,y+(h-smudge.line_height('ui12'))/2,U.fit(label,w-12,'ui12','bold'),'ui12',true,'center',not filled)
end
function U.hit(x,y,w,h,action,label)
 U.buttons[#U.buttons+1]={math.floor(x),math.floor(y),math.floor(w),math.floor(h),action,label or 'Link'}
end
function U.footer()
 if U.notice~='' then U.text(U.pad,U.bottom-25,U.fit(U.notice,U.w-2*U.pad,'small'),'small') end
 smudge.button_hints('Back','Select','Prev','Next')
end
function U.tap(x,y)
 for i,b in ipairs(U.buttons) do
  if smudge.in_rect(x,y,b[1],b[2],b[3],b[4]) then U.focus=i;b[5](b[7]);U.redraw();return true end
 end
 if y>=U.h-U.m.button_hints_height then
  local keys={'back','confirm','up','down'};on_button(keys[math.min(4,x*4//U.w+1)],true);return true
 end
end
function U.key(key)
 local n=#U.buttons;if n==0 then return end
 U.focus=math.max(1,math.min(n,U.focus))
 if key=='up' or key=='left' or key=='page_back' then U.focus=(U.focus-2)%n+1
 elseif key=='down' or key=='right' or key=='page_forward' then U.focus=U.focus%n+1
 elseif key=='confirm' then U.buttons[U.focus][5](U.buttons[U.focus][7]) end
 U.redraw()
end
-- UTF-8 byte caps keep saves and keyboard editing bounded on C3.
function U.edit(title,value,limit,done)
 U.buttons={};U.digits=nil
 collectgarbage('collect')
 U.editorDraw=U.editorDraw or dofile('editor.lua')
 U.editor={title=title,value=value or '',limit=limit,done=done,cursor=#(value or '')+1,shift=false,numeric=false};U.focus=1;U.redraw()
end
function U.closeEditor() U.buttons={};U.editor=nil;U.focus=1 end
function U.editDraw() U.editorDraw(U) end
function U.encode(s) return (s:gsub('[^%w%-_%.~]',function(c)return string.format('%%%02X',c:byte())end)) end
return U
