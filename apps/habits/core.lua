local M={}
function M.decode(s)
 if type(s)~='string' or #s>3000 then return end
 local names,history=s:match('^([^\n]*)\n(.*)$');if not names then return end
 local state={names={},history={}}
 for name in names:gmatch('[^\t]+') do
  if #name>40 or name:find('[%z\1-\31]') or not name:find('%S') or #state.names>=8 then return end
  state.names[#state.names+1]=name
 end
 local n=0
 for line in history:gmatch('[^\n]+') do
  local day,mask=line:match('^(%d+):(%x%x)$');day=tonumber(day);mask=mask and tonumber(mask,16)
  if not day or day<730000 or day>746000 or not mask or mask>255 or state.history[day] or n>=112 then return end
  state.history[day]=mask;n=n+1
 end
 if history~='' and history:sub(-1)~='\n' then return end
 return state
end
function M.encode(s,today)
 local out={table.concat(s.names,'\t')..'\n'}
 for day=today-111,today do if s.history[day] then out[#out+1]=day..':'..string.format('%02x',s.history[day])..'\n' end end
 return table.concat(out)
end
function M.checked(s,day,i) return ((s.history[day] or 0)&(1<<(i-1)))~=0 end
function M.toggle(s,day,i) s.history[day]=(s.history[day] or 0)~(1<<(i-1)) end
function M.streak(s,day,i)
 if not M.checked(s,day,i) then day=day-1 end
 local n=0;while n<112 and M.checked(s,day-n,i) do n=n+1 end;return n
end
function M.remove(s,i)
 table.remove(s.names,i)
 local low=(1<<(i-1))-1
 for day,mask in pairs(s.history) do s.history[day]=(mask&low)|((mask>>i)<<(i-1)) end
end
return M
