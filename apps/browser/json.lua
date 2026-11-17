-- Bounded data-only JSON. Integer arithmetic stays within production int32.
local J={null=setmetatable({}, {__json="null"})}
local array,object={__json="array"},{__json="object"}
local escapes={['"']='"',['\\']='\\',['/']='/',n='\n',t='\t'}
local literals={'true','false','null'}
function J.array(t)return setmetatable(t or {},array)end
function J.object(t)return setmetatable(t or {},object)end
function J.isarray(t)return type(t)=='table'and getmetatable(t)and getmetatable(t).__json=='array' end
local function valid(s)
 assert(utf8.len(s),'invalid UTF8')
 for i=1,#s do local b=s:byte(i);assert(b~=0 and (b>=32 or b==9 or b==10),'invalid control')end
 return s
end
function J.decode(s)
 assert(type(s)=='string'and #s<=49152,'JSON size')
 local p,n,nodes=1,#s,0
 local function ws()while p<=n and s:sub(p,p):match('[ \t\r\n]')do p=p+1 end end
 local function str()
  assert(s:sub(p,p)=='"','JSON string');p=p+1;local parts={};local start=p
  while p<=n do
   local b=s:byte(p)
   if b==34 then parts[#parts+1]=s:sub(start,p-1);p=p+1;return valid(table.concat(parts))end
   assert(b>=32,'JSON control')
   if b==92 then
    parts[#parts+1]=s:sub(start,p-1);p=p+1;local c=s:sub(p,p);p=p+1
    if c=='u'then
     local h=s:sub(p,p+3);assert(#h==4 and h:match('^%x%x%x%x$'),'JSON escape');local cp=tonumber(h,16);p=p+4
     if cp>=0xd800 and cp<=0xdbff then
      assert(s:sub(p,p+1)=='\\u','surrogate');p=p+2;h=s:sub(p,p+3);assert(h:match('^%x%x%x%x$'),'surrogate');local low=tonumber(h,16);p=p+4;assert(low>=0xdc00 and low<=0xdfff,'surrogate');cp=0x10000+(cp-0xd800)*1024+low-0xdc00
     else assert(cp<0xdc00 or cp>0xdfff,'surrogate')end
     parts[#parts+1]=utf8.char(cp)
    else assert(escapes[c],'JSON escape');parts[#parts+1]=escapes[c]end
    start=p
   else p=p+1 end
  end
  error('JSON unterminated')
 end
 local value
 value=function(depth)
  assert(depth<=12,'JSON depth');nodes=nodes+1;assert(nodes<=8192,'JSON nodes');ws();local c=s:sub(p,p)
  if c=='"'then return str()end
  if c=='{'or c=='['then
   p=p+1;local t=c=='['and J.array()or J.object();local close=c=='['and ']'or '}';ws()
   if s:sub(p,p)==close then p=p+1;return t end
   while true do
    if c=='{'then ws();local k=str();assert(t[k]==nil,'JSON duplicate');ws();assert(s:sub(p,p)==':','JSON colon');p=p+1;t[k]=value(depth+1)
    else t[#t+1]=value(depth+1)end
    ws();local sep=s:sub(p,p);p=p+1;if sep==close then return t end;assert(sep==',','JSON separator')
   end
  end
  for _,literal in ipairs(literals)do if s:sub(p,p+#literal-1)==literal then p=p+#literal;if literal=='null'then return J.null end;return literal=='true'end end
  local start=p;if c=='-'then p=p+1 end
  assert(s:sub(p,p):match('%d'),'JSON value');if s:sub(p,p)=='0'then p=p+1 else while s:sub(p,p):match('%d')do p=p+1 end end
  local text=s:sub(start,p-1);assert(#text<=7,'JSON integer');local num=tonumber(text);assert(num and math.abs(num)<=999999,'JSON integer');return num
 end
 local t=value(1);ws();assert(p==n+1,'JSON trailing');return t
end
local function quote(s)
 valid(s);return '"'..s:gsub('["\\\n\t]',function(c)return c=='\n'and '\\n'or c=='\t'and '\\t'or '\\'..c end)..'"'
end
function J.encode(t)
 local function enc(v,depth)
  assert(depth<=12,'JSON depth')
  if type(v)=='table'and getmetatable(v)and getmetatable(v).__json=='null'then return 'null'end
  if type(v)=='string'then return quote(v)end
  if type(v)=='boolean'then return tostring(v)end
  if type(v)=='number'then assert(math.type(v)=='integer'and math.abs(v)<=999999,'JSON integer');return tostring(v)end
  assert(type(v)=='table','JSON type');local a=J.isarray(v);local parts={}
  if a then for _,x in ipairs(v)do parts[#parts+1]=enc(x,depth+1)end
  else for k,x in pairs(v)do assert(type(k)=='string','JSON key');parts[#parts+1]=quote(k)..':'..enc(x,depth+1)end end
  return (a and '['or '{')..table.concat(parts,',')..(a and ']'or '}')
 end
 return enc(t,1)
end
return J
