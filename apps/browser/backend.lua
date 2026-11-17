-- Transport coordinator uses native ownership; callbacks only enqueue in main.
local P=dofile('protocol.lua');local J=P.json
local B={protocol=P,json=J}
function B.available()
 return (smudge.api_version or 0)>=7 and smudge.has_feature('browser_backend_public_v1')and type(smudge.browser_backend_request)=='function'and type(smudge.browser_request_id)=='function'
end
function B.configured()return B.available()end
function B.request(job)
 return smudge.browser_backend_request{path='/v1/'..job.endpoint,body=J.encode(job.payload),timeout_ms=job.endpoint=='open'and 2000 or job.endpoint=='image'and 15000 or 5000}
end
function B.job(endpoint,payload,extra)
 local job=extra or {};job.kind='backend';job.endpoint=endpoint;payload.v=1;job.payload=payload;return job
end
function B.read(e)
 assert(e.handle and math.type(e.size)=='integer'and e.size>=0 and e.size<=49152,'resource size')
 local parts={};for offset=0,e.size-1,8192 do local n=math.min(8192,e.size-offset);local s=smudge.http_read(e.handle,offset,n);assert(type(s)=='string'and #s==n,'resource read');parts[#parts+1]=s end
 local raw=table.concat(parts);return P.decode(raw,e.status),raw
end
return B
