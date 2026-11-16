-- Production before capture and structural contract, not a fake-font fixture.
assert(smudge.get_device()=='sticky' and utf8.len('Café')==4)
local read=smudge.read_file
smudge.read_file=function(path,n)if path:match('^art/')then return read(path,n)end end
smudge.write_file=function()return true end
smudge.get_date=function()return{year=2026,month=11,day=16,hour=12,min=0}end
smudge.time=function()return 100000 end
smudge.millis=function()return 1000 end
smudge.has_feature=function()return true end
smudge.http_request=function()return 1 end
smudge.http_cancel=function()end
local rect=smudge.rect;local banner=false
smudge.rect=function(x,y,w,h,fill,ink,...)
 if x==0 and y==0 and w==smudge.get_bounds()and h==76 and fill and ink then banner=true end
 return rect(x,y,w,h,fill,ink,...)
end
local app=dofile('main.lua');on_init();on_draw();capture('fresh.pgm')
assert(banner,'Hacker News production on_draw lacks original76px black banner')
on_exit();print('PASS original black banner through production callbacks')
