-- Appended to the existing input/SD/UI fixture prefix by combined.py.
for k,v in pairs(native_api)do smudge[k]=v end
smudge.api_version=7;smudge.millis=native_millis
local fixtureRead=smudge.read_file
smudge.read_file=function(path,count,offset)
 local value=fixtureRead(path,count,offset);if value~=nil then return value end
 local actual=native_resource_root..'/'..path:match('[^/]+$');local size=test_size(actual)
 if not size then return nil end
 local pieces={};offset=offset or 0;local last=math.min(size,offset+count)
 while offset<last do local part=test_read(actual,offset,math.min(8192,last-offset));pieces[#pieces+1]=part;offset=offset+#part end
 return table.concat(pieces)
end
smudge.browser_image_start=function(source,output,width,height)
 local bytes=assert(smudge.read_file(source,65536));files[output]=combined_decode(bytes,width,height);imageJob={output=output};return true
end
smudge.browser_image_blit=function(path,...)return combined_blit(assert(files[path]))end
local originalUpdate
local function update()native_dispatch();originalUpdate()end
local function waitFor(check,label)
 for _=1,3000 do native_pause();on_update();on_draw();if check()then draw();return end end
 error(label..': '..text)
end
local function menu(label)
 click('...')
 for _=1,10 do
  draw();for _,b in ipairs(modules['ui.lua'].buttons)do if b[6]:sub(1,#label)==label then click(label);return end end
  click('More')
 end;error('missing menu '..label..': '..text)
end
dofile('main.lua');originalUpdate=on_update;on_update=update;on_init();draw()
click('Address');edit('https://example.com/story');on_update()
waitFor(function()return text:find('Chunk 1/',1,true)end,'open/poll/ready')
assert(text:find('Beginning café',1,true),text)
assert(not modules['model.lua'],'Assisted loaded Direct parser')
menu('Save offline');waitFor(function()return text:find('Saved complete',1,true)end,'save every chunk')
menu('Address');edit('https://example.com/story#picture');on_update()
waitFor(function()return text:find('Original checkerboard',1,true)end,'image fragment')
click('Image');on_update()
waitFor(function()return imageJob~=nil end,'on-demand service PNG')
local persisted=false;for path in pairs(files)do if path:match('b7%-%d+%.png$')then persisted=true end end
assert(persisted,'service PNG did not persist after real codec')
menu('Find on page');edit('hello, world');on_update()
waitFor(function()return text:find('Beginning café',1,true)end,'styled-run Find first')
menu('Find on page');edit('hello, world');on_update()
waitFor(function()return text:find('FINAL_COMBINED_TAIL',1,true)end,'styled-run Find next across chunks')
menu('Address');edit('https://example.com/story#search');on_update()
waitFor(function()return text:find('GET search form',1,true)end,'form fragment')
click('GET search form');click('q:');edit('café words');click('Submit GET search');on_update()
waitFor(function()return text:find('Chunk 1/',1,true)end,'actual GET form navigation')
menu('Save offline');waitFor(function()return text:find('Saved complete',1,true)end,'save form result')
menu('Refresh page');menu('Cancel loading')
assert(text:find('Combined original',1,true),'cancel replaced old view')
for _=1,100 do native_pause();on_update()end
menu('Mode:');click('Offline');menu('Find on page');edit('FINAL_COMBINED_TAIL');draw()
assert(text:find('FINAL_COMBINED_TAIL',1,true),text)
w,h=800,480;draw();w,h=480,800;draw()
on_exit();for _=1,10 do native_dispatch()end
print('PASS combined Go extractor/owned job -> actual HTTP bytes -> native POST/resources -> real int32/float32 Lua -> render/B7/save/find/fragment/form/cancel/offline')
