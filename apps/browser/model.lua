local T,C,U=dofile('tokenizer.lua'),dofile('css.lua'),dofile('url.lua')
local M={url=function(s)return U.resolve(nil,s)end}
local entities={amp='&',lt='<',gt='>',quot='"',apos="'",nbsp=' ',mdash='—',ndash='–',hellip='…',lsquo='‘',rsquo='’',ldquo='“',rdquo='”',copy='©',bull='•'}
function M.decode(s)
 return (s:gsub('&(#?[%w]+);',function(e)
  if entities[e]then return entities[e]end
  local n=e:match('^#x(%x+)$')or e:match('^#X(%x+)$');n=n and tonumber(n,16)or tonumber(e:match('^#(%d+)$'))
  if n and n>0 and n<=0x10ffff and not(n>=0xd800 and n<=0xdfff)then return utf8.char(n)end
  return '&'..e..';'
 end))
end
-- A reluctant capture plus trailing %s* can rescan a long internal whitespace
-- run quadratically in native Lua. Keep preformatted-content trimming linear.
local function trim(s)
 local first,last=1,#s
 local function space(c)return c==32 or c and c>=9 and c<=13 end
 while first<=last and space(s:byte(first))do first=first+1 end
 while last>=first and space(s:byte(last))do last=last-1 end
 return s:sub(first,last)
end
local blockTags={p=true,div=true,article=true,main=true,section=true,header=true,footer=true,nav=true,aside=true,blockquote=true,pre=true,li=true,ul=true,ol=true,h1=true,h2=true,h3=true,h4=true,h5=true,h6=true,tr=true,table=true,form=true,figure=true,figcaption=true}
local void={img=true,br=true,hr=true,meta=true,link=true,input=true,base=true,wbr=true,source=true,area=true,embed=true}
local suppress={script=true,style=true,noscript=true,svg=true,canvas=true,template=true,iframe=true,object=true}
local clutter={nav=true,footer=true,aside=true,form=true,button=true,select=true}
local function feed(raw,url)
 local total=0
 local function append(old,value,limit)
  local room=math.min(limit-#old,65536-total);if room<=0 then return old end
  value=M.decode(value);if #value>room then value=value:sub(1,room);while #value>0 and not utf8.len(value)do value=value:sub(1,-2)end end
  total=total+#value;return old..value
 end
 local d={title='News feed',url=url,blocks={},links={},images={},forms={},stylesheets={},feed=true};local stack={};local item,field
 local ok,err=T.scan(raw,function(kind,name,attrs,selfClose)
  if kind=='open'then
   if #stack>=64 then error('depth_limit')end
   if not selfClose then stack[#stack+1]=name end
   if name=='item'or name=='entry'then item={title='',url='',date='',updated=''}end
   if item then
    if name=='title'then field='title'elseif name=='pubdate'or name=='published'then field='date'elseif name=='updated'then field='updated'
    elseif name=='link'then field='url';if attrs.href and (not attrs.rel or attrs.rel=='alternate')then item.url=attrs.href end
    else field=nil end
   elseif name=='title'then field='feedtitle'end
  elseif kind=='text'then
   if item and field then item[field]=append(item[field],name,field=='title'and 1024 or field=='url'and 512 or 128)
   elseif field=='feedtitle'then d.title=M.decode(name):sub(1,200)end
  elseif kind=='close'then
   if (name=='item'or name=='entry')and item then
    local target=U.resolve(url,item.url:match('^%s*(.-)%s*$'))
    if target and #d.links<100 then
     d.links[#d.links+1]={label=item.title,url=target};d.blocks[#d.blocks+1]={kind='h2',spans={{text=item.title,href=target,bold=true}}}
     if item.date==''then item.date=item.updated end
     if item.date~=''then d.blocks[#d.blocks+1]={kind='p',spans={{text=item.date}}}end
    end;item=nil
   end
   field=nil;stack[#stack]=nil
  end
 end)
 if not ok then return nil,err end
 if #d.links==0 then return nil,'No readable feed entries'end
 return d
end
-- Stylesheet discovery does not construct a throwaway article model. Full
-- validation and semantic extraction still happen exactly once in parse().
function M.discover(raw,url)
 local base=U.resolve(nil,url);local result={stylesheets={},styles={},raw=raw,url=base};local baseSet=false;local bytes=0
 if not base then return result end
 local ok,err=T.scan(raw,function(kind,name,a)
  if kind=='style'and bytes<24576 then local css=name:sub(1,24576-bytes);result.styles[#result.styles+1]=css;bytes=bytes+#css end
  if kind=='open'and name=='base'and a.href and not baseSet then base=U.resolve(base,M.decode(a.href))or base;baseSet=true end
  if kind=='open'and name=='link'and (a.rel or ''):lower()=='stylesheet'and a.href and #result.stylesheets<2 then
   local target=U.resolve(base,M.decode(a.href));if target then
    if #result.stylesheets==0 then result.styles[#result.styles+1]=false end
    result.stylesheets[#result.stylesheets+1]=target
   end
  end
 end)
 if not ok then return nil,err end
 result.base=base;return result
end
function M.parse(raw,url,options)
 options=options or {};url=U.resolve(nil,url or 'https://example.org/')
 local stream=type(raw)=='table'and type(raw.read)=='function'
 if not url or (type(raw)~='string'and not stream)or #raw==0 or #raw>1048576 or (stream and not raw:valid())or (not stream and (not utf8.len(raw)or raw:find('%z')))then return nil,'Invalid or oversized UTF-8 document'end
 if raw:sub(1,2048):lower():find('<rss[%s>]')or raw:sub(1,2048):lower():find('<feed[%s>]')then local ok,d,e=pcall(feed,raw,url);if not ok then return nil,tostring(d)end;return d,e end
 local discovery=options.discovery
 if not discovery or discovery.raw~=raw or discovery.url~=url then
  local err;discovery,err=M.discover(raw,url);if not discovery then return nil,err end
 end
 local styles={};local styleBytes=0;local sheetUrls=discovery.stylesheets;local base=discovery.base
 for _,part in ipairs(discovery.styles)do
  local value=part==false and options.css or part
  if value and styleBytes<24576 then value=value:sub(1,24576-styleBytes);styles[#styles+1]=value;styleBytes=styleBytes+#value end
 end
 local css=C.parse(table.concat(styles,'\n'));styles=nil
 local d={title='',url=url,blocks={},links={},images={},forms={},stylesheets=sheetUrls}
 local stack={};local active,bytes,spans= nil,0,0;local title=false;local preferred=false;local form
 -- IDs are bounded independently of blocks; defer empty wrappers to the next
 -- visible semantic content, then reindex only anchors retained by Reader mode.
 local ids,pendingIds,idCount={},{},0
 local function bindIds(b,s,p)
  for i,id in ipairs(pendingIds)do ids[id]={block=b,s=s,p=p};pendingIds[i]=nil end
 end
 local function boundary()active=nil end
 local function block(node,kind)
  if #d.blocks>=600 then error('block_limit')end
  local b={kind=kind or node.tag,spans={},id=node.attrs.id,style=node.style,preferred=node.preferred,rank=node.rank,clutter=node.clutter}
  d.blocks[#d.blocks+1]=b;active=b;bindIds(b,1,1);return b
 end
 local function add(text,node)
  if not node or node.hidden or node.style.hidden then return end
  text=M.decode(text)
  if node.style.pre then text=text:gsub('\t','    ')elseif text~='\n' then text=text:gsub('%s+',' ')end
  if text==''or (not active and not text:find('%S'))then return end
  if not active then block(node,node.kind or 'p')end
  if bytes+#text>65536 or spans>=2400 then d.truncated=true;return end
  local runs=active.spans;local last=runs[#runs]
  local merge=last and last.href==node.href and last.bold==node.style.bold and last.size==node.style.size and #last.text+#text<=2048
  if text:find('%S')then bindIds(active,merge and #runs or #runs+1,merge and #last.text+1 or 1)end
  if merge then last.text=last.text..text
  else runs[#runs+1]={text=text,href=node.href,bold=node.style.bold,size=node.style.size,pre=node.style.pre};spans=spans+1 end
  bytes=bytes+#text
 end
 local function process(kind,name,a,selfClose)
  local parent=stack[#stack]
  if kind=='text'then if title then d.title=(d.title..M.decode(name)):sub(1,200)else add(name,parent)end;return end
  if kind=='style'then return end
  if kind=='close'then
   if name=='title'then title=false end
   for i=#stack,1,-1 do if stack[i].tag==name then
    if blockTags[name]and not stack[i].style.inline then boundary()end
    if name=='form'then form=nil end
    for j=#stack,i,-1 do stack[j]=nil end;break
   end end;return
  end
  if #stack>=64 then error('depth_limit')end
  if name=='title'and parent and parent.tag=='head'and d.title==''then title=true end
  local node={tag=name,attrs=a};node.style=C.style(css,stack,node)
  node.hidden=(parent and parent.hidden)or suppress[name]or name=='head'
  node.rank=parent and parent.rank or 0
  if name=='article'then node.rank=2 elseif name=='main'and node.rank<1 then node.rank=1 end
  node.preferred=node.rank>0
  local marker=((a.class or '')..' '..(a.id or '')..' '..(a.name or '')..' '..(a['data-link-name']or '')..' '..(a['data-block']or '')):lower()
  node.clutter=(parent and parent.clutter)or clutter[name]or marker:find('cookie',1,true)or marker:find('advert',1,true)or marker:find('social',1,true)or marker:find('material%-icons')or marker:find('icon%-font')or marker:find('sharebutton',1,true)or marker:find('meta%-syndication')or marker:find('promolist',1,true)
  node.kind=blockTags[name]and name or parent and parent.kind
  node.href=parent and parent.href
  if name=='a'and a.href then node.href=U.resolve(base,M.decode(a.href))end
  if (blockTags[name]and not node.style.inline)or node.style.block then boundary()end
  if a.id and #a.id>0 and #a.id<=256 and not node.hidden and not node.style.hidden and not ids[a.id]and idCount<256 then
   ids[a.id]=true;idCount=idCount+1;pendingIds[#pendingIds+1]=a.id
  end
  if name=='meta'and a.content then
   local key=(a.name or a.property or ''):lower()
   if key=='author'or key=='article:published_time'or key=='date'then d[key]=M.decode(a.content):sub(1,200)end
  end
  if name=='br'then add('\n',node)end
  if name=='li'then add('• ',node)elseif name=='blockquote'then add('> ',node)
  elseif name=='td'or name=='th'then add(' | ',node)end
  if name=='img'and not node.hidden and not node.style.hidden then
   boundary();local src=U.resolve(base,M.decode(a.src or a['data-src']or ''))
   if src and #d.images<32 and (tonumber(a.width)or 100)>24 and (tonumber(a.height)or 100)>24 then
    local img={url=src,alt=M.decode(a.alt or 'Image'):sub(1,200),width=tonumber(a.width),height=tonumber(a.height)}
    d.images[#d.images+1]=img;local b=block(node,'image');b.image=img;boundary()
   end
  end
  if name=='form'and not node.hidden and not node.style.hidden and a.disabled==nil then
   form={url=U.resolve(base,M.decode(a.action or url)),fields={},method=(a.method or 'get'):lower(),label='Search'}
   if form.url and form.method=='get'and #d.forms<8 then d.forms[#d.forms+1]=form;local b=block(node,'form');b.form=form;boundary()else form=nil end
  elseif name=='input'and form and not node.hidden and not node.style.hidden and a.disabled==nil and a.name and #form.fields<12 and (a.type or '')~='password'and (a.type or '')~='file'then
   local kind=(a.type or 'text'):lower()
   if kind=='text'or kind=='search'or kind=='hidden'then form.fields[#form.fields+1]={name=M.decode(a.name),value=M.decode(a.value or ''),hidden=kind=='hidden'}end
  end
  -- Structural wrappers allocate no block until they contain actual text.
  if not void[name]and not selfClose then stack[#stack+1]=node end
 end
 local success,failure=pcall(function()local valid,e=T.scan(raw,process);if not valid then error(e)end end)
 if not success then return nil,tostring(failure)end
 -- Article/main wins when it actually contains useful text; otherwise paragraphs
 -- provide a density fallback. Simplified mode retains navigation and GET forms.
 local meaningful={0,0}
 for _,b in ipairs(d.blocks)do if b.preferred and not b.clutter then for _,r in ipairs(b.spans)do meaningful[b.rank]=meaningful[b.rank]+#r.text end end end
 local preferredRank=meaningful[2]>=80 and 2 or meaningful[1]>=80 and 1 or 0
 preferred=preferredRank>0
 local selected={};local seen={};local captions={}
 for _,b in ipairs(d.blocks)do
  local keep=options.reader==false or (not b.clutter and (not preferred or b.rank>=preferredRank))
  local plain={};for _,r in ipairs(b.spans)do plain[#plain+1]=r.text end;plain=trim(table.concat(plain))
  if plain=='•'or plain=='|'then keep=false end
  if b.kind=='figcaption'then if captions[plain]then keep=false else captions[plain]=true end end
  if keep and (#b.spans>0 or b.image or b.form)then
   selected[#selected+1]=b
   for _,r in ipairs(b.spans)do if r.href and not seen[r.href]and #d.links<200 then seen[r.href]=true;d.links[#d.links+1]={label=r.text,url=r.href}end end
  end
 end
 if #selected==0 then return nil,'No readable HTML; scripts, bot walls and paywalls are unsupported'end
 local metadata={}
 if d.author then metadata[#metadata+1]=d.author end
 if d['article:published_time']or d.date then metadata[#metadata+1]=d['article:published_time']or d.date end
 if #metadata>0 then table.insert(selected,1,{kind='metadata',spans={{text=table.concat(metadata,' • ')}}})end
 d.cssLimited=css.exhausted
 d.anchors={};local indices={}
 for i,b in ipairs(selected)do indices[b]=i end
 for id,a in pairs(ids)do if type(a)=='table'and indices[a.block]then d.anchors[id]={b=indices[a.block],s=a.s,p=a.p}end end
 d.blocks=selected;d.images={};d.forms={}
 for _,b in ipairs(selected)do if b.image then d.images[#d.images+1]=b.image end;if b.form then d.forms[#d.forms+1]=b.form end end
 if d.title==''then d.title=url:match('^https://([^/]+)')end
 if #d.blocks==0 then return nil,'No readable HTML; scripts, bot walls and paywalls are unsupported'end
 return d
end
function M.formURL(form,values)
 if not form or form.method~='get'then return end
 local pairs={}
 for i,f in ipairs(form.fields)do pairs[#pairs+1]=U.encode(f.name)..'='..U.encode(values and values[i]or f.value)end
 local root=form.url:gsub('#.*$','');return U.resolve(nil,root..(root:find('?',1,true)and '&'or '?')..table.concat(pairs,'&'))
end
return M
