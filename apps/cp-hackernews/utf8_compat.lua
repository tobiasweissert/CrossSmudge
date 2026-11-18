-- App-local UTF-8 subset for the production essential-library profile.
-- No provider text is rewritten; malformed sequences fail validation.
local U={}
local function decode(s,i)
 local a=s:byte(i);if not a then return end
 if a<128 then return a,i+1 end
 local n,cp,min
 if a>=194 and a<=223 then n,cp,min=2,a-192,128
 elseif a>=224 and a<=239 then n,cp,min=3,a-224,2048
 elseif a>=240 and a<=244 then n,cp,min=4,a-240,65536 else error('invalid UTF-8')end
 for k=1,n-1 do local b=s:byte(i+k);if not b or b<128 or b>191 then error('invalid UTF-8')end;cp=cp*64+b-128 end
 if cp<min or cp>1114111 or(cp>=55296 and cp<=57343)then error('invalid UTF-8')end
 return cp,i+n
end
function U.codes(s)
 local i=1;return function()if i>#s then return end;local at=i;local cp;cp,i=decode(s,i);return at,cp end
end
function U.len(s)
 local n=0;local ok=pcall(function()for _ in U.codes(s)do n=n+1 end end);if ok then return n end;return nil
end
function U.char(...)
 local out={};for _,c in ipairs({...})do
  assert(c>=0 and c<=1114111 and c%1==0 and not(c>=55296 and c<=57343),'invalid codepoint')
  if c<128 then out[#out+1]=string.char(c)
  elseif c<2048 then out[#out+1]=string.char(192+c//64,128+c%64)
  elseif c<65536 then out[#out+1]=string.char(224+c//4096,128+c//64%64,128+c%64)
  else out[#out+1]=string.char(240+c//262144,128+c//4096%64,128+c//64%64,128+c%64)end
 end;return table.concat(out)
end
function U.offset(s,n,i)
 i=i or(n>=0 and 1 or #s+1);if i<0 then i=#s+i+1 end
 if n==0 then while i>1 and s:byte(i)and s:byte(i)>=128 and s:byte(i)<=191 do i=i-1 end;return i end
 if n>0 then for k=1,n-1 do if i>#s then return nil end;local _;_,i=decode(s,i)end;return i<=#s+1 and i or nil end
 for k=1,-n do i=i-1;if i<1 then return nil end;while i>1 and s:byte(i)>=128 and s:byte(i)<=191 do i=i-1 end end;return i
end
return U
