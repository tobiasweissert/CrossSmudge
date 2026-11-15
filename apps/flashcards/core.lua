local M={}
function M.validText(s,n) return type(s)=='string' and #s>0 and #s<=n and not s:find('[%z\1-\31]') and s:find('%S')~=nil end
function M.decode(s)
 if type(s)~='string' or #s>3800 then return end
 local title,rest=s:match('^([^\n]+)\n(.*)$');if not M.validText(title,32) then return end
 local cards={}
 for line in rest:gmatch('[^\n]+') do
  local q,a=line:match('^([^\t]+)\t([^\t]+)$')
  if not M.validText(q,160) or not M.validText(a,160) or #cards>=24 then return end
  cards[#cards+1]={q,a}
 end
 if rest~='' and rest:sub(-1)~='\n' then return end
 return {title=title,cards=cards}
end
function M.encode(d)
 local lines={d.title..'\n'}
 for _,c in ipairs(d.cards) do lines[#lines+1]=c[1]..'\t'..c[2]..'\n' end
 return table.concat(lines)
end
return M
