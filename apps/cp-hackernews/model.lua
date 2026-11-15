local J=dofile('json.lua')
local M={}
function M.ids(raw)
  local t=J.decode(raw)
  if type(t)~='table' or #t<1 or #t>500 then return end
  local seen={}
  for k,v in pairs(t) do
    if math.type(k)~='integer' or k<1 or k>#t or math.type(v)~='integer' or v<1 or seen[v] then return end
    seen[v]=true
  end
  return t
end
function M.item(raw)
  local t=J.decode(raw)
  if type(t)~='table' or math.type(t.id)~='integer' or t.id<1 or t.deleted or t.dead
    or t.type~='story' or type(t.title)~='string' or #t.title>1024 then return end
  for _,key in ipairs({'url','text','by'}) do
    if t[key]~=nil and type(t[key])~='string' then return end
  end
  return t
end
function M.plain(s)
  s=(s or ''):gsub('<[Pp][^>]*>','\n'):gsub('<[^>]*>','')
  local entities={amp='&',lt='<',gt='>',quot='"',apos="'",nbsp=' '}
  s=s:gsub('&(#?[%w]+);',function(e)
    if entities[e] then return entities[e] end
    local cp=e:match('^#x(%x+)$')
    cp=cp and tonumber(cp,16) or tonumber(e:match('^#(%d+)$'))
    if cp and cp>0 and cp<=0x10ffff and not(cp>=0xd800 and cp<=0xdfff) then return utf8.char(cp) end
    return '&'..e..';'
  end)
  return (s:gsub('[%z\1-\8\11-\31\127]',' '))
end
function M.article_url(url)
  if type(url)~='string' or #url>360 or url:find('[%s%c\\#]') then return end
  local host=url:match('^https?://([^/]+)')
  if not host or not host:match('^[%a%d][%a%d%.%-]+%.[%a]+$') then return end
  return 'https://r.jina.ai/'..url
end
return M
