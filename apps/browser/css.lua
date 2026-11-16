local C={}
local inherited={'bold','size','pre'}
function C.declarations(s)
 local d={}
 for k,v in s:gmatch('([%a-]+)%s*:%s*([^;{}]+)')do
  k=k:lower();v=v:lower():match('^%s*(.-)%s*$');local important=v:find('!important',1,true)~=nil;v=v:gsub('%s*!important','')
  if #v<=80 then d[k]={v,important}end
 end
 return d
end
function C.parse(s)
 local rules={};s=s:sub(1,24576):gsub('/%*.-%*/','')
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
 for selectors,body in s:gmatch('([^{}]+){([^{}]*)}')do
  if #rules>=128 then break end
  for selector in selectors:gmatch('[^,]+')do
   selector=selector:match('^%s*(.-)%s*$')
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
