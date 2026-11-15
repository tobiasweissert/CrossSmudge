click('Enter address');enter('example.com');on_update();assert(REQUEST.options.url=='https://r.jina.ai/https://example.com/');deliver(fixture('page.txt'));assert(TEXT:find('documentation',1,true))
click('Next');click('Previous');click('Links');click('[1]');on_update();assert(REQUEST.options.url:find('iana.org',1,true));deliver(nil,'body_limit');assert(TEXT:find('8 KB',1,true))
on_button('back',true);draw();restart();assert(TEXT:find('Offline cache',1,true))
click('...');click('Refresh');on_update();on_button('back',true);assert(CANCELLED);on_http{id=1,body='late',status=200};draw();assert(TEXT:find('Cancelled',1,true))
do local M=MODULES['model.lua'];assert(not M.url('https://localhost/'));assert(not M.url('https://127.0.0.1/'));assert(not M.url('https://a.com:443/'));assert(not M.url('https://u:p@a.com/'));assert(not M.url('https://a.com/%0d'));assert(not M.parse('\255'))
local d=assert(M.parse(string.rep('word ',150)..'[Link](https://example.com/)'));local pos,seen=1,0
repeat local lines,n=M.page(d.text,pos,WIDTH-40,5);assert(n>pos);seen=seen+#lines;pos=n;collectgarbage('collect')until pos>#d.text
assert(seen>5 and #d.links==1) end
collectgarbage('collect')
-- Exercise the actual callback and SD journal at the transport's 8 KB ceiling.
click('...');click('Refresh page');on_update();deliver(TEST_BODY())
assert(TEXT:find('Public text via Jina Reader',1,true))
restart();assert(TEXT:find('Offline cache',1,true))
click('...');click('Refresh');on_update();deliver(TEST_BODY(1))
restart();assert(TEXT:find('Offline cache',1,true))
-- Text entry must remain usable with a full cached page resident.
click('...');click('Enter address');on_button('back',true);on_update();on_button('back',true);draw();assert(TEXT:find('Page 1',1,true))
