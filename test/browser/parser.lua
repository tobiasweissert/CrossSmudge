local load=dofile
function dofile(n)return load('apps/browser/'..n)end
local M=dofile('model.lua')
local d=assert(M.parse([[<!doctype html><html><head><title>Original title</title><style>.hide {display:none} article p {font-weight:bold} #hero {font-size:20px} </style></head><body><nav><a href="/bad">Menu clutter</a></nav><main><article><h1 id="hero">A &amp; B</h1><p>A useful UTF-8 article: café &#x2014; news. <a href="../next?q=1&amp;x=2#part">Follow this</a></p><p class="hide">secret hidden</p><script>if(a < b) {fake='danger';}</script><img src="/image?id=1" alt="A scene"><form action="/search" method="get"><input name="q" value="hello world"><input name="sort" value="new"><button>Search</button></form></article></main></body></html>]],'https://example.org/news/story'))
assert(d.title=='Original title')
local function text(doc)local a={};for _,b in ipairs(doc.blocks)do for _,r in ipairs(b.spans or {})do a[#a+1]=r.text end end;return table.concat(a,' ')end
assert(text(d):find('café — news.',1,true));assert(not text(d):find('danger'));assert(not text(d):find('secret'));assert(not text(d):find('Menu clutter'))
assert(d.links[1].url=='https://example.org/next?q=1&x=2#part')
assert(#d.images==1 and d.images[1].url=='https://example.org/image?id=1')
local simple=assert(M.parse('<nav><a href="/nav">Go</a></nav><p>Hi</p>','https://example.org/',{reader=false}));assert(text(simple):find('Go'))
local f=assert(M.parse('<rss><channel><title>Live feed</title><item><title>Fresh &amp; real</title><link>https://news.example.org/a</link><pubDate>Today</pubDate></item></channel></rss>','https://news.example.org/feed'));assert(f.links[1].url=='https://news.example.org/a' and text(f):find('Fresh & real',1,true))
local U=dofile('url.lua');assert(U.resolve('https://e.org/a/b','../c#x')=='https://e.org/c#x');assert(U.resolve('https://e.org/a/b','?q=hi')=='https://e.org/a/b?q=hi');assert(not U.resolve('https://e.org/','javascript:bad'));assert(not U.resolve('https://e.org/','https://127.0.0.1/'));assert(not U.resolve('https://e.org/','/%zz'))
assert(not M.parse('\255','https://example.org/'))
print('PASS tokenizer/reader/entities/links/feed/CSS production model')
local hidden=assert(M.parse('<style>p{display:none!important}</style><article><p style="display:block">Must hide</p><h1>Visible heading</h1></article>','https://example.org/'))
assert(not text(hidden):find('Must hide',1,true),'important declaration lost to normal inline style')
local wrappers=assert(M.parse(string.rep('<div></div>',700)..'<article><p>Useful text retained after many empty wrappers.</p></article>','https://example.org/'));assert(text(wrappers):find('Useful text',1,true))
