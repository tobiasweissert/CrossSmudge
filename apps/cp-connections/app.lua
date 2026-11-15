-- Local API4 preview: hardware activation waits for an atomic storage backend.
local A,J,G,S=dofile('archive.lua'),dofile('json.lua'),dofile('game.lua'),dofile('save.lua')
local importer,game,save,request,queued,closed,target
local cursor,status,page=1,'Open Latest or Today from the saved archive',1
local entries={}
local function redraw() smudge.request_update() end
local function scanning(day)
  target=day;importer=A.new(smudge.download_read,J.decode,day);status='Scanning archive';redraw()
end
local function persist()
  if save and not save:write(game:encode()) then status=status..'; save failed' end
end
function on_init() closed=false;scanning(nil) end
function on_update()
  if closed then return end
  if queued and not request then
    queued=false
    if not smudge.has_feature('https_download_file') then status='API4 atomic storage unavailable';redraw();return end
    local err
    request,err=smudge.http_download{url='https://raw.githubusercontent.com/Eyefyre/NYT-Connections-Answers/main/connections.json',max_bytes=2097152,timeout_ms=30000}
    status=request and 'Downloading archive' or ('Download: '..tostring(err));redraw()
  elseif importer then
    local done,p,err=importer:step()
    if done then
      importer=nil
      if p then
        save=S.open(function(raw) return G.restore(p,raw)~=nil end)
        game=save.body and G.restore(p,save.body) or G.new(p,smudge.random(0,65535))
        cursor=1;status=p.date..' / '..game.result
      else status='Archive: '..tostring(err) end
      redraw()
    end
  end
end
function on_http(e)
  if closed or not request or e.id~=request then return end
  request=nil
  if e.error or e.kind~='download' or e.status~=200 then status='Download: '..tostring(e.error or e.status);redraw()
  else scanning(target) end
end
function on_exit()
  closed=true;queued=false;importer=nil
  if request then smudge.http_cancel(request);request=nil end
  game=nil;save=nil
end
local function choices()
  local e={}
  if game then
    if game.result=='playing' then
      for i,t in ipairs(game.order) do
        e[#e+1]={label=(game.chosen[t] and '[x] ' or '[ ] ')..game.puzzle.answers[(t-1)//4+1].members[(t-1)%4+1],tile=i}
      end
      e[#e+1]={label='Submit four',action='submit'};e[#e+1]={label='Shuffle',action='shuffle'}
    end
    if game.result~='playing' then
      for _,g in ipairs(game.puzzle.answers) do e[#e+1]={label=g.group..': '..table.concat(g.members,', ')} end
    else
      for _,i in ipairs(game.solved) do e[#e+1]={label='Solved: '..game.puzzle.answers[i].group} end
    end
  end
  e[#e+1]={label='Latest saved puzzle',action='latest'}
  e[#e+1]={label='Today (device date)',action='today'}
  e[#e+1]={label='Download archive',action='download'}
  if request then e[#e+1]={label='Cancel download',action='cancel'} end
  return e
end
local function activate()
  local e=choices()[cursor];if not e then return end
  if e.action=='cancel' and request then smudge.http_cancel(request)
  elseif request or importer then return
  elseif e.tile then game:toggle(e.tile)
  elseif e.action=='submit' then status=game:submit()..' / mistakes '..game.mistakes..'/4';persist()
  elseif e.action=='shuffle' then game:shuffle(smudge.random(0,65535));persist()
  elseif e.action=='latest' then scanning(nil)
  elseif e.action=='today' then local d=smudge.get_date();scanning(string.format('%04d-%02d-%02d',d.year,d.month,d.day))
  elseif e.action=='download' then queued=true end
  redraw()
end
function on_button(b)
  if closed then return end
  if b=='back' then on_exit();smudge.exit();return end
  local n=#choices()
  if b=='down' or b=='right' or b=='page_forward' then cursor=cursor%n+1
  elseif b=='up' or b=='left' or b=='page_back' then cursor=(cursor-2)%n+1
  elseif b=='confirm' then activate() end
  redraw()
end
local layout
local function draw_wrapped(x,y,width,text,maxlines)
  local lines=smudge.wrapped_text(text,width,'ui12','regular',maxlines)
  local height=smudge.line_height('ui12')
  for i,s in ipairs(lines) do smudge.text(x,y+(i-1)*height,s,'ui12') end
end
function on_draw()
  if closed then return end
  local w,h=smudge.get_bounds();local m=smudge.get_metrics();local line=smudge.line_height('ui12')
  local pad=m.content_side_padding or 10;local top=m.header_height+m.top_padding+line*2
  local bottom=h-m.button_hints_height-line
  local rows=math.max(1,(bottom-top)//(line*2))
  entries=choices();cursor=math.min(cursor,#entries);page=(cursor-1)//rows
  layout={top=top,height=line*2,rows=rows,w=w,h=h,footer=h-m.button_hints_height}
  smudge.clear();smudge.header('Connections preview')
  -- Wrap rather than split UTF-8 bytes; renderer handles glyph boundaries.
  draw_wrapped(pad,m.header_height+m.top_padding,w-2*pad,status,2)
  for r=1,rows do
    local at=page*rows+r;local e=entries[at]
    if e then draw_wrapped(pad,top+(r-1)*line*2,w-2*pad,(at==cursor and '> ' or '')..e.label,2) end
  end
  if smudge.has_touch() then
    for i,s in ipairs({'Back','Select','Prev','Next'}) do smudge.text((i-1)*w/4+4,h-line,s,'ui12') end
  else smudge.button_hints('Back','Select','Prev','Next') end
end
function on_touch(x,y,event)
  if closed or event~='tap' or not layout or x<0 or y<0 or x>=layout.w or y>=layout.h then return end
  if y>=layout.footer then on_button(({'back','confirm','up','down'})[x*4//layout.w+1])
  elseif y>=layout.top and y<layout.top+layout.rows*layout.height then
    local at=page*layout.rows+(y-layout.top)//layout.height+1
    if entries[at] then cursor=at;activate() end
  end
end
