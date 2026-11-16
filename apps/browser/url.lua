-- HTTPS only. DNS/public-address enforcement is repeated by native TLS per hop.
local U={}
function U.resolve(base,ref)
 if type(ref)~='string' or #ref>512 then return end
 ref=ref:match('^%s*(.-)%s*$')
 if ref==''and base then return U.resolve(nil,base)end
 if ref:find('[%z\1-\32\127-\255\\]') then return end
 local p=1
 while true do local at=ref:find('%%',p);if not at then break end
  local h=ref:sub(at+1,at+2);if #h~=2 or h:find('[^%x]')then return end
  local n=tonumber(h,16);if n<32 or n==127 or n==92 then return end;p=at+3
 end
 if ref:sub(1,2)=='//'then ref='https:'..ref end
 if not ref:match('^https://')then
  if ref:match('^[%a][%w+.-]*:')then return end
  if not base then ref='https://'..ref
  else
   local root,path=base:match('^(https://[^/]+)(/[^#]*)');if not root then return end
   path=path:gsub('%?.*$','')
   if ref:sub(1,1)=='#'then ref=base:gsub('#.*$','')..ref
   elseif ref:sub(1,1)=='?'then ref=root..path..ref
   elseif ref:sub(1,1)=='/'then ref=root..ref
   else ref=root..(path:match('^(.*/)')or '/')..ref end
  end
 end
 local host,rest=ref:match('^https://([^/?#]+)(.*)$');if not host then return end
 host=host:lower()
 if #host>127 or not host:find('%.')or host:find('[^%w%.%-]')or not host:match('%.[a-z][a-z]+$')or host:find('%.%.')then return end
 for label in host:gmatch('[^.]+')do if #label>63 or label:match('^%-')or label:match('%-$')then return end end
 if host:sub(1,1)=='.'or host:sub(-1)=='.'then return end
 if rest==''or rest:match('^[?#]')then rest='/'..rest end
 local path,tail=rest:match('^([^?#]*)(.*)$');local parts={};local at=2
 while at<=#path+1 do
  local slash=path:find('/',at,true);local part=path:sub(at,slash and slash-1 or #path)
  if part=='..'then table.remove(parts);if not slash then parts[#parts+1]=''end
  elseif part=='.'then if not slash then parts[#parts+1]=''end
  else parts[#parts+1]=part end
  if not slash then break end;at=slash+1
 end
 local normalized='/'..table.concat(parts,'/')
 rest=normalized..tail
 local wire=rest:gsub('#.*$','');if #wire>384 then return end
 local result='https://'..host..rest;if #result>512 then return end;return result
end
function U.wire(url)return url and url:gsub('#.*$','')end
function U.encode(s)return (s:gsub('[^%w%-_%.~]',function(c)return string.format('%%%02X',c:byte())end))end
return U
