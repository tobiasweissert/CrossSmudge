-- Streaming, quote-aware HTML/XML tokenizer. No DOM or token array retained.
local T={}
function T.scan(s,emit)
 local at,count=1,0
 local function text(first,last)
  while first<=last do
   local finish=math.min(last,first+4095);local value=s:sub(first,finish)
   while #value>0 and not utf8.len(value)do finish=finish-1;value=s:sub(first,finish)end
   if finish<last then local amp=value:match('()&[#%w]*$');if amp and #value-amp<32 and amp>1 then finish=first+amp-2;value=s:sub(first,finish)end end
   if finish<first then return end
   emit('text',value);first=finish+1
  end
 end
 while at<=#s do
  count=count+1;if count>16000 then return nil,'token_limit'end
  local lt=s:find('<',at,true)
  if not lt then text(at,#s);break end
  if lt>at then text(at,lt-1)end
  if s:sub(lt,lt+3)=='<!--'then local last=s:find('-->',lt+4,true);at=last and last+3 or #s+1
  elseif s:sub(lt,lt+8)=='<![CDATA['then local last=s:find(']]>',lt+9,true);text(lt+9,last and last-1 or #s);at=last and last+3 or #s+1
  else
   local p,quote=lt+1,nil
   while p<=#s do local c=s:sub(p,p)
    if quote then if c==quote then quote=nil end
    elseif c=='"'or c=="'"then quote=c
    elseif c=='>'then break end;p=p+1
    -- Scan oversized attribute tags without retaining their payload.
   end
   if p>#s then break end
   local raw=s:sub(lt+1,math.min(p-1,lt+4096));local closing=raw:match('^%s*/')~=nil
   local name,finish=raw:match('^%s*/?%s*([%w:_-]+)()')
   if name then
    name=name:lower();local attrs={};local n=0;local i=finish
    if not closing then
     while i<=#raw do
      local _,last,key=raw:find('^%s*([%w:_-]+)',i);if not last then i=i+1 else
       i=last+1;key=key:lower();local _,eq=raw:find('^%s*=%s*',i);local value=''
       if eq then i=eq+1;local c=raw:sub(i,i)
        if c=='"'or c=="'"then local e=raw:find(c,i+1,true);if not e then break end;value=raw:sub(i+1,e-1);i=e+1
        else local e=raw:find('[%s>]',i);value=raw:sub(i,e and e-1 or #raw);i=e or #raw+1 end
       end
       n=n+1;if n<=32 and #value<=1024 and attrs[key]==nil then attrs[key]=value end
      end
     end
    end
    emit(closing and 'close'or 'open',name,attrs,s:sub(math.max(lt+1,p-32),p-1):match('/%s*$')~=nil)
    if not closing and (name=='script'or name=='style')then
     -- Raw text tags: '<' inside JavaScript/CSS never creates HTML nodes.
     local a,b=s:find('</%s*'..name:gsub('%a',function(c)return '['..c..c:upper()..']'end)..'%s*>',p+1)
     if name=='style'then emit('style',s:sub(p+1,math.min(p+24576,a and a-1 or #s)))end
     emit('close',name);p=b or #s
    end
   end
   at=p+1
  end
 end
 return true
end
return T
