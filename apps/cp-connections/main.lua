-- Keep unsupported devices out before compiling/loading the game modules.
local device=smudge.get_device()
local supported=device=='sticky' or device=='x4pro'
if supported and smudge.has_feature('https_download_file') then
  dofile('app.lua')
else
  function on_init() smudge.request_update() end
  function on_draw()
    smudge.clear();smudge.header('Connections preview')
    local w=smudge.get_bounds();local m=smudge.get_metrics()
    local message=supported and 'API4 atomic storage is unavailable in this firmware.' or 'This development preview requires Sticky or X4 Pro.'
    local lines=smudge.wrapped_text(message,w-40,'ui12','regular',4)
    for i,s in ipairs(lines) do smudge.text(20,m.header_height+m.top_padding+i*smudge.line_height('ui12'),s,'ui12') end
    smudge.button_hints('Back','','','')
  end
  function on_button(b) if b=='back' then smudge.exit() end end
  function on_touch(x,y,event) if event=='tap' then smudge.exit() end end
end
