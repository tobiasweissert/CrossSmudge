local host=dofile
function dofile(p)return host('apps/browser/'..p)end
local J,P=dofile('json.lua'),dofile('protocol.lua')
local model={v=1,state='ready',doc=string.rep('a',32),url='https://example.org/',title='Synthetic',lang='en',dir='ltr',mode='reader',chunks=1,chunk=0,truncated=false,blocks=J.array{{id=1,kind='p',runs=J.array{{text='é Tail'}}}},anchors=J.array{{id='top',block=1,run=1,byte=1}}}
local contexts={
 {'ltr rtl auto',function(t,v)t.dir=v end},
 {'reader simplified',function(t,v)t.mode=v end},
 {'p h1 h2 h3 li quote pre metadata caption',function(t,v)t.blocks[1].kind=v end},
 {'text search hidden',function(t,v)t.blocks=J.array{{id=1,kind='form',form={action='https://example.org/',fields=J.array{{name='q',value='',type=v}}}}};t.anchors=J.array()end},
 {'invalid_request unauthorized forbidden address_denied tls_failed upstream_denied unsupported_content too_large unreadable busy timeout cancelled expired not_found',function(t,v)for k in pairs(t)do t[k]=nil end;t.v=1;t.error=v;t.retryable=false end,400}
}
for _,c in ipairs(contexts)do
 local values={};for s in c[1]:gmatch('%S+')do values[#values+1]=s end
 local function check(v,expected)
  local t=J.decode(J.encode(model));c[2](t,v);local ok=pcall(P.decode,J.encode(t),c[3]or 200)
  assert(ok==expected,'enum '..c[1]..' value '..tostring(v))
 end
 for _,v in ipairs(values)do check(v,true);check(v..' ',false);check(' '..v,false);check(v:upper(),false)end
 for i=1,#values-1 do check(values[i]..' '..values[i+1],false)end
 for _,v in ipairs({'','unknown',true,1,J.null,'ead','ut','h','text search hidden','timeout cancelled'})do check(v,false)end
end
print('PASS CB-LUA-001 exact members in every SRP1 enum context')
