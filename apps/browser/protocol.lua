local J,URL=dofile('json.lua'),dofile('url.lua')
local P={json=J}
local function fields(t,required,optional)
 assert(type(t)=='table'and t~=J.null and not J.isarray(t),'object')
 local allowed={};for k in required:gmatch('%S+')do allowed[k]=true;assert(t[k]~=nil,'missing '..k)end
 for k in (optional or ''):gmatch('%S+')do allowed[k]=true end
 for k in pairs(t)do assert(allowed[k],'unknown '..tostring(k))end
end
local function integer(n,lo,hi)assert(math.type(n)=='integer'and n>=lo and n<=hi,'integer');return n end
local function text(s,n)assert(type(s)=='string'and #s<=n,'string');return s end
local function id(s)assert(type(s)=='string'and #s==32 and s:match('^[0-9a-f]+$'),'id');return s end
local function url(s)text(s,512);local normalized=URL.resolve(nil,s);assert(normalized and s:sub(1,8)=='https://'and not s:find('%s'),'URL');return normalized end
local function array(t,n)assert(J.isarray(t)and #t<=n,'array');return t end
local function boolean(b)assert(type(b)=='boolean','boolean');return b end
local function enum(s,values)
 assert(type(s)=='string','enum')
 for member in values:gmatch('%S+')do if s==member then return s end end
 error('enum')
end
function P.position(t)
 fields(t,'v found',t.found and 'chunk block run byte'or '')
 assert(t.v==1);boolean(t.found)
 if t.found then for k,limits in pairs({chunk={0,63},block={1,1024},run={1,512},byte={1,2049}})do assert(t[k]~=nil);integer(t[k],limits[1],limits[2])end end
 return t
end
local function positionBoundary(block,run,byte)
 if not block then return false end
 if block.image or block.form then return run==1 and byte==1 end
 local r=block.spans[run]
 return r and byte<=#r.text+1 and (byte==#r.text+1 or utf8.offset(r.text,0,byte)==byte)
end
function P.ready(t)
 fields(t,'v state doc url title lang dir mode chunks chunk truncated blocks anchors','reason')
 assert(t.v==1 and t.state=='ready');id(t.doc);t.url=url(t.url);assert(not t.url:find("#",1,true));text(t.title,200);text(t.lang,35);enum(t.dir,'ltr rtl auto');enum(t.mode,'reader simplified');integer(t.chunks,1,64);integer(t.chunk,0,t.chunks-1);boolean(t.truncated)
 if t.reason~=nil then assert(t.truncated);text(t.reason,64);assert(t.reason:match('^[a-z_]+$'))end
 local total=#t.title+#t.lang+#t.url;local runs,hrefs,links=0,{},0;local map={};local doc={title=t.title,blocks={},anchors={},images={},forms={},truncated=t.truncated,srp=t}
 local function count(s,n)text(s,n);total=total+#s;assert(total<=8192,'decoded text');return s end
 for _,b in ipairs(array(t.blocks,64))do
  integer(b.id,1,1024);assert(not map[b.id],'duplicate block');map[b.id]=#doc.blocks+1
  local out={id=b.id,spans={}};doc.blocks[#doc.blocks+1]=out
  if b.kind=='image'then
   fields(b,'id kind image');fields(b.image,'asset alt width height');local im=b.image;id(im.asset);count(im.alt,200);integer(im.width,1,999999);integer(im.height,1,999999);out.image={asset=im.asset,alt=im.alt,width=im.width,height=im.height};doc.images[#doc.images+1]=out.image;assert(#doc.images<=16)
  elseif b.kind=='form'then
   fields(b,'id kind form');local f=b.form;fields(f,'action fields');f.action=url(f.action);count(f.action,512);out.form={method='get',url=f.action,fields={}}
   for _,v in ipairs(array(f.fields,12))do fields(v,'name value type');count(v.name,256);count(v.value,256);enum(v.type,'text search hidden');out.form.fields[#out.form.fields+1]={name=v.name,value=v.value,hidden=v.type=='hidden'}end
   doc.forms[#doc.forms+1]=out.form;assert(#doc.forms<=8)
  else
   fields(b,'id kind runs','continued');enum(b.kind,'p h1 h2 h3 li quote pre metadata caption');if b.continued~=nil then boolean(b.continued)end
   for _,r in ipairs(array(b.runs,256))do
    fields(r,'text','size bold pre href');count(r.text,2048);if r.size~=nil then integer(r.size,0,2)end;if r.bold~=nil then boolean(r.bold)end;if r.pre~=nil then boolean(r.pre)end
    if r.href then r.href=url(r.href);if not hrefs[r.href]then hrefs[r.href]=true;links=links+1;assert(links<=64)end end
    runs=runs+1;assert(runs<=256);out.spans[#out.spans+1]={text=r.text,size=r.size or 0,bold=r.bold or false,pre=r.pre or false,href=r.href}
   end
  end
 end
 for _,a in ipairs(array(t.anchors,64))do
  fields(a,'id block run byte');count(a.id,256);assert(not doc.anchors[a.id],'duplicate anchor');integer(a.block,1,1024);integer(a.run,1,512);integer(a.byte,1,2049)
  local b=map[a.block];assert(positionBoundary(b and doc.blocks[b],a.run,a.byte),'anchor boundary');doc.anchors[a.id]={b=b,s=a.run,p=a.byte}
 end
 doc.blockMap=map;return doc
end
function P.decode(raw,status)
 local t=J.decode(raw)
 if status>=400 then assert(#raw<=2048,'error size');fields(t,'v error retryable');assert(t.v==1);enum(t.error,'invalid_request unauthorized forbidden address_denied tls_failed upstream_denied unsupported_content too_large unreadable busy timeout cancelled expired not_found');boolean(t.retryable);return t end
 if status==202 then fields(t,'v state job retry_ms');assert(t.v==1 and t.state=='pending');id(t.job);integer(t.retry_ms,500,2000);return t end
 assert(status==200,'status')
 if t.state=='ready'then return P.ready(t)end
 if t.state=='cancelled'then fields(t,'v state');assert(t.v==1);return t end
 return P.position(t)
end
function P.anchor(doc,pos)
 local b=doc.blockMap[pos.block]
 assert(positionBoundary(b and doc.blocks[b],pos.run,pos.byte),'position boundary');return {b=b,s=pos.run,p=pos.byte}
end
function P.formURL(form)
 local parts={};for _,f in ipairs(form.fields)do parts[#parts+1]=URL.encode(f.name)..'='..URL.encode(f.value)end
 local root=form.url:gsub('#.*$','');return URL.resolve(nil,root..(root:find('?',1,true)and '&'or '?')..table.concat(parts,'&'))
end
return P
