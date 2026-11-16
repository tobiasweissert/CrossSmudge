local C={}
local inherited={'bold','size','pre'}
local function whitespace(c)return c==32 or c and c>=9 and c<=13 end
local function keychar(c)return c and (c==45 or c>=65 and c<=90 or c>=97 and c<=122)end
local function trim(s)
 local first,last=1,#s
 while first<=last and whitespace(s:byte(first))do first=first+1 end
 while last>=first and whitespace(s:byte(last))do last=last-1 end
 return s:sub(first,last)
end
local function important(s)
 local at=s:find('!important',1,true);if not at then return s,false end
 local parts={};local pos=1
 while at do
  local first=at;while first>pos and whitespace(s:byte(first-1))do first=first-1 end
  parts[#parts+1]=s:sub(pos,first-1);pos=at+10;at=s:find('!important',pos,true)
 end
 parts[#parts+1]=s:sub(pos);return table.concat(parts),true
end
function C.declarations(s)
 local d={};local pos=1
 while pos<=#s do
  local last=s:find('[;{}]',pos)or #s+1;local part=s:sub(pos,last-1)
  local colon=part:find(':',1,true)
  while colon and colon<#part do
   local finish=colon-1
   while finish>0 and whitespace(part:byte(finish))do finish=finish-1 end
   local first=finish;while first>0 and keychar(part:byte(first))do first=first-1 end
   if first<finish then
    local key=part:sub(first+1,finish):lower();local value=trim(part:sub(colon+1):lower())
    local priority;value,priority=important(value)
    if #value<=80 then d[key]={value,priority}end
    break
   end
   colon=part:find(':',colon+1,true)
  end
  pos=last+1
 end
 return d
end
local function stripComments(s)
 local parts={};local pos=1
 while pos<=#s do
  local first=s:find('/*',pos,true)
  if not first then break end
  local last=s:find('*/',first+2,true)
  if not last then break end
  parts[#parts+1]=s:sub(pos,first-1);pos=last+2
 end
 parts[#parts+1]=s:sub(pos);return table.concat(parts)
end
local function blocks(s)
 local pos,start,open=1,1,nil
 return function()
  while pos<=#s do
   local at=s:find('[{}]',pos);if not at then return end
   pos=at+1
   if s:byte(at)==123 then
    if open then start=open+1 end;open=at
   else
    local first,brace=start,open;start=pos;open=nil
    if brace and brace>first then return s:sub(first,brace-1),s:sub(brace+1,at-1)end
   end
  end
 end
end
function C.parse(s)
 local rules={};s=stripComments(s:sub(1,24576))
 -- Unsupported at-rules are inert, including nested media/supports/keyframes.
 local cleaned={};local p=1
 while p<=#s do
  local at=s:find('@',p,true)
  if not at then cleaned[#cleaned+1]=s:sub(p);break end
  cleaned[#cleaned+1]=s:sub(p,at-1)
  local i=at+1;local depth=0;local quote
  while i<=#s do local c=s:sub(i,i)
   if quote then if c==quote and s:sub(i-1,i-1)~='\\'then quote=nil end
   elseif c=='"'or c=="'"then quote=c
   elseif c=='{'then depth=depth+1
   elseif c=='}'then depth=depth-1;if depth==0 then i=i+1;break end
   elseif c==';'and depth==0 then i=i+1;break end;i=i+1
  end;p=i
 end
 s=table.concat(cleaned)
 for selectors,body in blocks(s)do
  if #rules>=128 then break end
  for selector in selectors:gmatch('[^,]+')do
   selector=trim(selector)
   if #selector<=120 and not selector:find('[>%+~:%[%]@]')then
    local chain={};local specificity=0
    for part in selector:gmatch('%S+')do chain[#chain+1]=part
     for _ in part:gmatch('#')do specificity=specificity+100 end
     for _ in part:gmatch('%.')do specificity=specificity+10 end
     if part:match('^[%a]')then specificity=specificity+1 end
    end
    if #chain>0 and #chain<=4 and #rules<128 then rules[#rules+1]={chain=chain,specificity=specificity,values=C.declarations(body)}end
   end
  end
 end
 return rules
end
local function matches(node,part)
 local tag=part:match('^([%w_-]+)');if tag and node.tag~=tag then return false end
 for id in part:gmatch('#([%w_-]+)')do if node.attrs.id~=id then return false end end
 local classes=' '..(node.attrs.class or ''):gsub('%s+',' ')..' '
 for cls in part:gmatch('%.([%w_-]+)')do if not classes:find(' '..cls..' ',1,true)then return false end end
 return true
end
function C.style(rules,stack,node)
 local parent=stack[#stack];local out={}
 if parent then for _,k in ipairs(inherited)do out[k]=parent.style[k]end;out.hidden=parent.style.hidden end
 local tag=node.tag
 if tag:match('^h[1-6]$')then out.bold=true;out.size=tag=='h1'and 2 or 1 end
 if tag=='strong'or tag=='b'or tag=='th'then out.bold=true end
 if tag=='pre'or tag=='code'then out.pre=true end
 local function boundedMatch(node,selector)
  rules.work=(rules.work or 0)+1
  if rules.work>50000 then rules.work=50000;rules.exhausted=true;return false end
  return matches(node,selector)
 end
 local values,weights={},{}
 local function apply(d,weight)
  for k,pair in pairs(d)do local w=weight+(pair[2]and 1000000 or 0)
   if not weights[k]or w>=weights[k]then weights[k]=w;values[k]=pair[1]end
  end
 end
 for i,rule in ipairs(rules)do
  if rules.exhausted then break end
  local chain=rule.chain
  if boundedMatch(node,chain[#chain])then
   local j=#chain-1
   for n=#stack,1,-1 do if rules.exhausted then break end;if j>0 and boundedMatch(stack[n],chain[j])then j=j-1 end end
   if j==0 then apply(rule.values,rule.specificity*129+i)end
  end
 end
 apply(C.declarations(node.attrs.style or ''),1000*129)
 if values.display=='none'or values.visibility=='hidden'or node.attrs.hidden~=nil then out.hidden=true end
 if values['font-weight']then out.bold=values['font-weight']=='bold'or (tonumber(values['font-weight'])or 0)>=600 end
 local size=tonumber((values['font-size']or ''):match('^(%d+)px$'));if size then out.size=size>=20 and 2 or size>=16 and 1 or 0 end
 out.margin=math.min(20,tonumber((values['margin-bottom']or values.margin or ''):match('^(%d+)'))or 0)
 out.border=values.border and not values.border:find('none',1,true)and 1 or nil
 out.background=values['background-color']or values.background
 out.inline=values.display=='inline';out.block=values.display=='block'
 return out
end
return C
