collectgarbage('incremental',105,250,0)
local path='apps/cp-connections/'
local f=assert(io.open(os.getenv('CONNECTIONS_FIXTURE') or '/root/github/.host-data/crosssmudge/lua-network-apps/public-fixtures-parent/connections.body','rb'))
local total=f:seek('end');local readmax=0;local labels={};local files={};local nextid=0;local cancelled=0
smudge={
 download_read=function(o,n)assert(n<=1024);readmax=math.max(readmax,n);f:seek('set',o);return f:read(n) or '',total end,
 read_file=function(p,n)return files[p] end,write_file=function(p,s)files[p]=s;return true end,
 random=function(a,b)assert(a==0 and b==65535);return 123 end,request_update=function()end,has_feature=function()return true end,
 get_device=function()return 'sticky' end,
 get_date=function()return {year=2026,month=11,day=15}end,get_bounds=function()return 480,800 end,
 get_metrics=function()return {header_height=40,top_padding=8,button_hints_height=30}end,
 line_height=function()return 20 end,clear=function()labels={}end,header=function()end,
 text=function(x,y,s)assert(type(s)=='string');labels[#labels+1]=s end,
 wrapped_text=function(s,w,font,style,max)assert(type(s)=='string' and w>0 and max<=2);return {s}end,
 has_touch=function()return true end,button_hints=function()end,exit=function()end,
 http_download=function(o)assert(o.max_bytes==2097152 and not o.body and not o.headers);nextid=nextid+1;return nextid end,
 http_cancel=function(id)assert(id==nextid);cancelled=cancelled+1 end
}
local original=dofile;dofile=function(p)return original(path..p)end
assert(loadfile(path..'main.lua'))();on_init();for i=1,1500 do on_update() end
on_draw();assert(table.concat(labels):find('2026%-11%-15'))
-- Select first word, then request download using the bottom menu entry.
on_button('confirm');on_button('up');on_button('confirm');on_update();assert(nextid==1)
on_http{id=999,status=200,kind='download',size=total};on_exit();assert(cancelled==1)
on_http{id=1,status=200,kind='download',size=total};on_update();on_draw()
f:close();print('PASS actual app full archive/lifecycle/stale-event/cancel; max read '..readmax)
