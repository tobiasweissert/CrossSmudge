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
    if #value<=80 then
     local function put(k,v)if not d[k]or not d[k][2]or priority then d[k]={v,priority}end end
     put(key,value)
     if key=='margin'then
      local a={};for v in value:gmatch('%S+')do a[#a+1]=v end
      if #a>=1 and #a<=4 then put('margin-right',a[2]or a[1]);put('margin-left',a[4]or a[2]or a[1])end
     end
    end
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
  parts[#parts+1]=s:sub(pos,first-1);parts[#parts+1]=string.rep(' ',last+2-first);pos=last+2
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
    if brace and brace>first then return s:sub(first,brace-1),s:sub(brace+1,at-1),at end
   end
  end
 end
end
-- Validate the whole bounded compound; never extract partial constraints from
-- unsupported namespaces, attributes, pseudo selectors or combinators.
local function compound(part)
 local tag,at=part:match('^([%w_-]+)()');local specificity=tag and 1 or 0
 if not at then at=part:sub(1,1)=='*'and 2 or 1 end
 while at<=#part do
  local marker,name,nextAt=part:sub(at):match('^([.#])([%w_-]+)()')
  if not name then return end
  specificity=specificity+(marker=='#'and 100 or 10);at=at+nextAt-1
 end
 if #part>0 then return specificity end
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
  end;cleaned[#cleaned+1]=string.rep(' ',i-at);p=i
 end
 s=table.concat(cleaned)
 for selectors,body,order in blocks(s)do
  if #rules>=128 then break end
  for selector in selectors:gmatch('[^,]+')do
   selector=trim(selector)
   if #selector<=120 then
    local chain={};local specificity=0
    for part in selector:gmatch('%S+')do
     local weight=compound(part);if not weight then chain={};break end
     chain[#chain+1]=part;specificity=specificity+weight
    end
    if #chain>0 and #chain<=4 and #rules<128 then rules[#rules+1]={chain=chain,specificity=specificity,order=order,sheet=1,values=C.declarations(body)}end
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
-- A separate, bounded display index preserves text box boundaries beyond the
-- presentation CSS prefix. Only simple selectors are supported here; no site
-- markers or guessed word boundaries. Read raw stylesheet ranges in 8 KiB pieces.
-- Separate fields avoid float32 rounding and incompatible index/prefix weights.
local function wins(a,b)
 if not b then return true end
 for _,k in ipairs{'priority','specificity','sheet','order'}do
  if a[k]~=b[k]then return a[k]>b[k]end
 end
 return true
end
function C.layout(index,range)
 local header,body='', '';local depth=0;local quote,comment,previous=nil,false,''
 local overflow,nested,escape=false,false,false;local scanned=index.scanned or 0
 local function append(c)
  if depth==0 then if #header<1024 then header=header..c else overflow=true end
  elseif depth==1 then if #body<4096 then body=body..c else overflow=true end end
 end
 local function rule(order)
  if overflow or nested or header:find('@',1,true)then return end
  local declarations=C.declarations(body)
  if not(declarations.display or declarations['margin-left']or declarations['margin-right'])then return end
  for selector in header:gmatch('[^,]+')do
   selector=trim(selector)
   if #selector<=120 and selector:match('^[.#]?[%w_-]+$')then
    local old=index[selector]
    if old or (index.count or 0)<256 then
     local entry=old or {properties={}};entry.properties=entry.properties or {}
     for _,key in ipairs{'display','margin-left','margin-right'}do
      local pair=declarations[key]
      if pair then
       local candidate={value=pair[1],priority=pair[2]and 1 or 0,specificity=selector:sub(1,1)=='#'and 100 or selector:sub(1,1)=='.'and 10 or 1,sheet=range.sheet or 1,order=order}
       local previous=entry.properties[key]
       if wins(candidate,previous)then entry.properties[key]=candidate end
      end
     end
     local display=entry.properties.display;if display then entry.value=display.value end
     index[selector]=entry
     if not old then index.count=(index.count or 0)+1 end
    end
   end
  end
 end
 for pos=range.first,range.last,8192 do
  if scanned>=1048576 then index.exhausted=true;break end
  local part=range.raw:sub(pos,math.min(range.last,pos+8191));scanned=scanned+#part
  for i=1,#part do local c=part:sub(i,i)
   if comment then if previous=='*'and c=='/'then comment=false;c=''end
   elseif escape then escape=false
   elseif c=='\\'then escape=true;if not quote then append('?')end
   elseif quote then
    if c==quote then quote=nil end
   elseif previous=='/'and c=='*'then
    comment=true
    if depth==0 then header=header:sub(1,-2)elseif depth==1 then body=body:sub(1,-2)end
    append(' ')
   elseif c=='"'or c=="'"then
    quote=c;append('?')
   elseif c=='{'then if depth>0 then nested=true end;depth=depth+1
   elseif c=='}'then
    if depth==1 then rule(pos+i-range.first);header,body,overflow,nested='','',false,false end
    depth=math.max(0,depth-1)
   elseif depth==0 then
    if c==';'then header='';overflow=false else append(c)end
   elseif depth==1 then append(c)end
   previous=c
  end
 end
 index.scanned=scanned
end
local function display(index,node,key)
 local best
 local function pick(selector)local entry=index and index[selector];local v=entry and entry.properties and entry.properties[key];if v and wins(v,best)then best=v end end
 pick(node.tag);pick('#'..(node.attrs.id or ''))
 for cls in (node.attrs.class or ''):gmatch('%S+')do pick('.'..cls)end
 return best
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
 local function apply(d,specificity,sheet,order)
  for k,pair in pairs(d)do
   local candidate={priority=pair[2]and 1 or 0,specificity=specificity,sheet=sheet,order=order}
   if wins(candidate,weights[k])then weights[k]=candidate;values[k]=pair[1]end
  end
 end
 for i,rule in ipairs(rules)do
  if rules.exhausted then break end
  local chain=rule.chain
  if boundedMatch(node,chain[#chain])then
   local j=#chain-1
   for n=#stack,1,-1 do if rules.exhausted then break end;if j>0 and boundedMatch(stack[n],chain[j])then j=j-1 end end
   if j==0 then apply(rule.values,rule.specificity,rule.sheet,rule.order)end
  end
 end
 apply(C.declarations(node.attrs.style or ''),1000,0,0)
 -- Both bounded paths use the same cascade and original stylesheet positions.
 for _,key in ipairs{'display','margin-left','margin-right'}do
  local candidate=display(rules.layout,node,key)
  if candidate and wins(candidate,weights[key])then values[key]=candidate.value;weights[key]=candidate end
 end
 -- Only explicit positive literal lengths justify a text gap. Dynamic, auto,
 -- percentages, zero and negative margins remain inert (no layout guesswork).
 local function positive(v)
  local n,unit=(v or ''):match('^([%d.]+)(%a+)$')
  return n and tonumber(n)and tonumber(n)>0 and(unit=='px'or unit=='em'or unit=='rem')or false
 end
 out.gapBefore=positive(values['margin-left']);out.gapAfter=positive(values['margin-right'])
 if values.display=='none'or values.visibility=='hidden'or node.attrs.hidden~=nil then out.hidden=true end
 if values['font-weight']then out.bold=values['font-weight']=='bold'or (tonumber(values['font-weight'])or 0)>=600 end
 local size=tonumber((values['font-size']or ''):match('^(%d+)px$'));if size then out.size=size>=20 and 2 or size>=16 and 1 or 0 end
 out.margin=math.min(20,tonumber((values['margin-bottom']or values.margin or ''):match('^(%d+)'))or 0)
 out.border=values.border and not values.border:find('none',1,true)and 1 or nil
 out.background=values['background-color']or values.background
 out.inline=values.display=='inline'
 out.block=values.display=='block'or values.display=='flex'or values.display=='grid'or values.display=='flow-root'or values.display=='list-item'or values.display=='table'or values.display=='-webkit-box'
 return out
end
return C
