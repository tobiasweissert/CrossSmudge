-- Only device UI, files and time are doubled. HTTP comes from registerApi.
local host=dofile
local readAsset=TEST_READ_ASSET or function(p,n)
  local f=assert(io.open('apps/'..APP..'/'..p,'rb'));local data=f:read(n);f:close();return data
end
dofile=function(p) assert(p:match('^[a-z_]+%.lua$')); return host('apps/'..APP..'/'..p) end
local files={}
rendered=''
smudge={
  read_file=function(p,n)
    if p:match('^art/lato%-%w+%.bin$') then return readAsset(p,n) end
    return files[p] and files[p]:sub(1,n) end,
  write_file=function(p,s) files[p]=s; return true end,
  get_date=function() return {year=2026,month=11,day=15,hour=12,min=0} end,
  request_update=function() end,exit=function() end,
  has_feature=function() return true end, has_touch=function() return true end,
  get_bounds=function() return 800,480 end,
  get_metrics=function() return {header_height=40,top_padding=10,button_hints_height=40,content_side_padding=20} end,
  line_height=function() return 20 end,text_width=function(s) return (assert(utf8.len(s)))*8 end,
  clear=function() rendered='' end,header=function() end,button_hints=function() end,
  text=function(x,y,s) assert(x>=0 and y>=50 and y<480);rendered=rendered..s..'\n' end,
  bitmap=function()end,rect=function()end,rect_dither=function()end,rounded_rect=function()end,line=function()end,time=function()return 0 end,
  millis=function() return 0 end,random=function(a,b) return b end
}
