local M={}
function M.url(s)
 if type(s)~='string' or #s>300 or s:find('[%z\1-\32\127\\#]') then return end
 if not s:match('^https://') then s='https://'..s end
 local host,path=s:match('^https://([^/]+)(/.*)$')
 if not host then host=s:match('^https://([^/]+)$');path='/'end
 if not host or #host>127 or host:find('[^%w%.%-]') or not host:find('%.')or host:match('%.%.')or host:match('^[%.%-]')or host:match('[%.%-]$')or not host:match('%.[%a][%a]+$')then return end
 for label in host:gmatch('[^.]+')do if #label>63 or label:match('^%-')or label:match('%-$')then return end end
 if path:find('%%[01]%x')or path:find('%%7[fF]')then return end
 return 'https://'..host:lower()..path
end
function M.valid(raw,start)
 return not (type(raw)~='string' or #raw==0 or #raw-(start or 1)+1>8192 or raw:find('[%z\1-\8\11\12\14-\31\127]') or not utf8.len(raw))
end
function M.parse(raw,start)
 if not M.valid(raw,start)then return end
 local _,header=raw:find('Markdown Content:%s*\n')
 local text=raw
 local links={};local budget=0
 -- Keep the validated source instead of copying the entire response through
 -- gsub buffers. Markdown stays readable; supported links have a separate menu.
 for label,url in text:gmatch('%[([^%]]-)%]%((https://[^%)%s]+)%)')do
  url=M.url(url)
  local duplicate=false
  for _,link in ipairs(links)do if link[2]==url then duplicate=true;break end end
  if url and not duplicate and #links<8 and budget+#url+math.min(80,#label)<=1024 then
   budget=budget+#url+math.min(80,#label)
   if #label>80 then label=label:sub(1,utf8.offset(label,0,81)-1)end
   links[#links+1]={label,url}
   if #links==8 then break end
  end
 end
 return {text=text,links=links,start=header and header+1 or (start or 1)}
end
-- Wrap one screen at a time. Offsets point into immutable UTF-8 text.
function M.page(text,start,width,rows)
 local lines={};local p=start
 for row=1,rows do
  if p>#text then break end
  local begin,lastSpace,last=p,nil,p-1
  while p<=#text do
   local byte=text:byte(p);local n=byte<128 and 1 or byte<224 and 2 or byte<240 and 3 or 4
   if byte==10 then last=p-1;p=p+1;break end
   if byte==32 then lastSpace=p end
   if smudge.text_width(text:sub(begin,p+n-1),'ui12')>width then
    if lastSpace and lastSpace>begin then last=lastSpace-1;p=lastSpace+1
    elseif p==begin then last=p+n-1;p=p+n end
    break
   end
   last=p+n-1;p=p+n
  end
  lines[#lines+1]=text:sub(begin,last)
 end
 return lines,p
end
function M.cache(raw)
 if type(raw)~='string'then return end
 local split=raw:find('\n',1,true);if not split then return end
 local url=raw:sub(1,split-1)
 if M.url(url)and M.valid(raw,split+1)then return{url=url,start=split+1}end
end
return M
