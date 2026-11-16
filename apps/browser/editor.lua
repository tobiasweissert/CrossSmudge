-- Loaded only while entering text; released on Done/Cancel.
return function(U)
 local e=U.editor;U.begin(e.title,'Back cancels')
 local kw=(U.w-16)//10;local kh=math.min(60,(U.bottom-U.top-90)//5)
 local y=U.bottom-5*(kh+4)
 local visible=math.max(1,(y-U.top-8)//smudge.line_height('ui12'))
 local draft=e.value:sub(1,e.cursor-1)..'|'..e.value:sub(e.cursor)
 local lines=smudge.wrapped_text(draft,U.w-2*U.pad,'ui12','regular',100).lines
 local caret=1;for i,line in ipairs(lines)do if line:find('|',1,true)then caret=i;break end end
 e.view=math.max(1,math.min(e.view or 1,caret))
 if caret>=e.view+visible then e.view=caret-visible+1 end
 for i=e.view,math.min(#lines,e.view+visible-1)do U.text(U.pad,U.top+(i-e.view)*smudge.line_height('ui12'),lines[i])end
 local function insert(c) if #e.value+#c<=e.limit then e.value=e.value:sub(1,e.cursor-1)..c..e.value:sub(e.cursor);e.cursor=e.cursor+#c end end
 local rows=e.numeric and {'1234567890','-/:.,?!@&','_+=()%#;'} or {'qwertyuiop','asdfghjkl','zxcvbnm'}
 for row,letters in ipairs(rows) do
  for i=1,#letters do
   local c=letters:sub(i,i);if e.shift then c=c:upper() end
   local x=(U.w-#letters*(kw+1))//2+(i-1)*(kw+1)
   U.button(c,x,y+(row-1)*(kh+4),kw,kh,insert,false,c)
  end
 end
 local labels={e.numeric and 'abc' or '123',e.shift and 'Lower' or 'Shift','Space','<','>','Delete'}
 for i,label in ipairs(labels) do
  U.button(label,8+(i-1)*(U.w-16)/6,y+3*(kh+4),(U.w-24)/6,kh,function()
   if i==1 then e.numeric=not e.numeric
   elseif i==2 then e.shift=not e.shift
   elseif i==3 then insert(' ')
   elseif i==4 then e.cursor=e.cursor>1 and (utf8.offset(e.value,-1,e.cursor)or 1)or 1
   elseif i==5 then e.cursor=e.cursor<=#e.value and (utf8.offset(e.value,2,e.cursor)or #e.value+1)or #e.value+1
   end
  end)
 end
 -- Replace Delete action: remove exactly one UTF-8 codepoint, including ASCII.
 U.buttons[#U.buttons][5]=function()
  if e.cursor>1 then local p=utf8.offset(e.value,-1,e.cursor)or 1;e.value=e.value:sub(1,p-1)..e.value:sub(e.cursor);e.cursor=p end
 end
 U.button('Done',8,y+4*(kh+4),U.w-16,kh,function() U.closeEditor();e.done(e.value);U.redraw() end,true)
 smudge.button_hints('Cancel','Key','Prev','Next')
end
