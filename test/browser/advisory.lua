local host=dofile;function dofile(n)return host('apps/browser/'..n)end
local M=dofile('model.lua')
local function text(doc)local a={};for _,b in ipairs(doc.blocks)do for _,r in ipairs(b.spans or {})do a[#a+1]=r.text end end;return table.concat(a,' ')end
local d=assert(M.parse('<html><body><div><main><article><p>'..string.rep('Lead ',30)..'</p><div data-large="'..string.rep('x',5000)..'"></div><p>TAIL MUST SURVIVE</p></article></main></div></body></html>','https://example.org/'))
assert(text(d):find('TAIL MUST SURVIVE',1,true),'oversized tag lost ancestry')
d=assert(M.parse('<style>@media print { p { display:none } } @media(max-width:1px){h1{display:none}}</style><article><h1>Title</h1><p>Screen text stays visible</p></article>','https://example.org/'))
assert(text(d):find('Screen text stays visible',1,true),'print CSS executed')
d=assert(M.parse('<form style="display:none" action="/hidden"><input name="q"></form><p>Visible</p>','https://example.org/',{reader=false}));assert(#d.forms==0,'hidden form visible')
d=assert(M.parse('<feed><title>Feed</title><entry><title>Story</title><link href="https://example.org/story"/><published>2026-01-01</published><updated>2026-01-02</updated></entry></feed>','https://example.org/'));assert(not text(d):find('2026%-01%-012026'),'Atom dates concatenated')
local title=table.concat((function()local a={};for i=1,160 do a[i]=string.rep('x',512)..'<!--split-->'end;return a end)())
d=assert(M.parse('<rss><channel><item><title>'..title..'</title><link>https://example.org/story</link></item></channel></rss>','https://example.org/'));assert(#d.links[1].label<=1024,'feed field bound bypass')
assert(not M.parse('<rss>'..string.rep('<x>',70)..'text'..string.rep('</x>',70)..'</rss>','https://example.org/'),'feed depth bypass')
d=assert(M.parse('<article><p>Useful</p><figure><figcaption>One caption</figcaption></figure><figure><figcaption>One caption</figcaption></figure><ul><li><span></span></li></ul><aside><h2>Most viewed</h2></aside></article>','https://example.org/'))
local txt=text(d);local _,n=txt:gsub('One caption','');assert(n==1,'duplicate captions');for _,b in ipairs(d.blocks)do assert(not text{blocks={b}}:match('^%s*•%s*$'),'empty bullet')end
print('PASS second independent audit regressions')
local C=dofile('css.lua');local rules=C.parse(string.rep('#absent div {font-size:12px}',128));local stack={}
for i=1,63 do stack[i]={tag='div',attrs={},style={}}end
for i=1,1000 do C.style(rules,stack,{tag='div',attrs={}})end
assert(rules.exhausted and rules.work<=50000,'CSS work must be bounded')

assert(not M.parse('<head><meta name="author" content="Publisher"></head><script>boot()</script>','https://example.org/'),'metadata-only script page accepted')
