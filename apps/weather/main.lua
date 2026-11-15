local U=dofile('ui.lua');collectgarbage('collect')
local M=dofile('model.lua');collectgarbage('collect')
local S=dofile('storage.lua');collectgarbage('collect')
local location,forecast,pending,active,cities,detail,closed
local status='Refresh to fetch weather'
local function queue(kind,url)
 if active or pending then status='Request in progress';return end
 pending={kind=kind,url=url};status='Queued';U.redraw()
end
local function refresh()
 local url='https://api.open-meteo.com/v1/forecast?latitude='..string.format('%.4f',location.lat)..'&longitude='..string.format('%.4f',location.lon)..'&current=temperature_2m,apparent_temperature,weather_code,wind_speed_10m&hourly=temperature_2m,weather_code&forecast_hours=16&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=5&timezone=auto&format=csv'
 queue('forecast',url)
end
local function search()
 if active or pending then status='Wait or press Back to cancel';return end
 U.edit('Find a city','',48,function(s)if s:find('%S')then queue('cities','https://geocoding-api.open-meteo.com/v1/search?name='..U.encode(s)..'&count=3&language=en&format=json')end end)
end
function on_init()
 closed=false;local saved=S.load(M.cache)
 location=saved and saved.location or {lat=47.3769,lon=8.5417,name='Zurich'}
 forecast=saved and saved.forecast;collectgarbage('collect')
 if forecast then status='Cached: '..forecast.current.time end
 U.redraw()
end
function on_update()
 if closed or active or not pending then return end
 local work=pending;pending=nil
 if not smudge.has_feature('https_async')then status='HTTPS unavailable in simulator';U.redraw();return end
 local id,err=smudge.http_request{url=work.url,headers={Accept=work.kind=='forecast'and 'text/csv'or 'application/json'}}
 if id then active={id=id,kind=work.kind};status='Loading...'
 else status='Request failed: '..tostring(err)end;U.redraw()
end
function on_http(e)
 if closed or not active or e.id~=active.id then return end
 local kind=active.kind;active=nil
 if e.error or e.status~=200 then status=e.error=='time_untrusted'and 'Trust verified UTC in App Settings'or('Fetch failed: '..tostring(e.error or e.status));U.redraw();return end
 if kind=='cities'then
  forecast=nil;U.digits=nil;collectgarbage('collect');cities=M.cities(e.body);status=cities and (#cities==0 and 'No matching city' or 'Choose a city')or 'Invalid city response'
 else
  U.digits=nil;collectgarbage('collect')
  local nextForecast=M.forecast(e.body);collectgarbage('collect')
  if nextForecast then
   forecast=nextForecast
   local saved=S.save(M.encodeLocation(location)..'\n'..e.body)
   status=saved and ('Updated '..forecast.current.time:sub(12,16))or 'Updated; cache save failed'
  else status='Invalid forecast; previous data kept'end
 end
 U.focus=1;U.redraw()
end
local function temp(n)return string.format('%.0f',n)..'o'end
function on_draw()
 if U.editor then U.editDraw();return end
 U.begin(location.name,status)
 local p,w,t,b=U.pad,U.w,U.top,U.bottom
 if cities and #cities>0 then
  for i,city in ipairs(cities)do U.button(city.name,p,t+(i-1)*48,w-2*p,42,function()
   location=city;forecast=nil;cities=nil;U.save(S,M.encodeLocation(location)..'\n');refresh()
  end)end
 else
  if forecast then
   local c=forecast.current;local y
   if detail and w>U.h then
    U.text(p,t+10,temp(c.temperature_2m)..'  '..M.condition(c.weather_code),'ui12',true)
    y=t+40
   else
    local nh=math.min(145,(b-t)*0.24)
    U.number(t+10,temp(c.temperature_2m),nh)
    y=t+nh+25;U.text(p,y,M.condition(c.weather_code),'ui12',true);y=y+30
   end
   U.text(p,y,'Feels '..temp(c.apparent_temperature)..'  Wind '..string.format('%.0f',c.wind_speed_10m)..' km/h','small')
   y=y+36
   if detail then
    for i=2,5 do
     if y+30>b-90 then break end
     U.text(p,y,forecast.daily.time[i]:sub(6),'small',true)
     U.text(p+70,y,M.condition(forecast.daily.weather_code[i]),'small')
     U.text(w-p,y,temp(forecast.daily.temperature_2m_max[i])..' / '..temp(forecast.daily.temperature_2m_min[i]),'small',true,'right')
     smudge.line(p,y+27,w-p,y+27);y=y+40
    end
   else
    local count=w>U.h and 5 or 3
    local cw=(w-2*p-8*(count-1))/count
    for i=1,count do local hour=1+(i-1)*3
     local x=p+(i-1)*(cw+8);local ch=math.min(120,b-y-100)
     if ch>=60 then
      smudge.rounded_rect(math.floor(x),y,math.floor(cw),ch,10,i==1,1)
      U.text(x+cw/2,y+10,forecast.hourly.time[hour]:sub(12,16),'small',false,'center',i~=1)
      U.text(x+cw/2,y+34,temp(forecast.hourly.temperature_2m[hour]),'ui12',true,'center',i~=1)
      if ch>90 then U.text(x+cw/2,y+70,M.condition(forecast.hourly.weather_code[hour]),'small',false,'center',i~=1)end
     end
    end
   end
  else U.wrap('No forecast yet. Select Refresh. Wi-Fi and trusted UTC are required.',p,t+35,w-2*p,4)end
  local bw=(w-2*p-16)//3
  U.button('Refresh',p,b-67,bw,42,refresh,true)
  U.button('Location',p+bw+8,b-67,bw,42,search)
  U.button(detail and 'Overview'or 'Details',p+2*(bw+8),b-67,bw,42,function()detail=not detail end)
  U.text(p,b-17,'Weather: Open-Meteo (CC BY 4.0)','small')
 end
 U.footer()
end
local function cancel()
 pending=nil;if active then smudge.http_cancel(active.id);active=nil end
end
function on_button(key,pressed)
 if not pressed then return end
 if U.editor then if key=='back'then U.closeEditor();U.redraw()else U.key(key)end;return end
 if key=='back'then
  if active or pending then cancel();status='Cancelled'
  elseif cities then
   cities=nil;S.seq=0;local saved=S.load(M.cache);forecast=saved and saved.forecast
  else closed=true;smudge.exit()end
 else U.key(key)end;U.redraw()
end
function on_tap(x,y)U.tap(x,y)end
function on_swipe(dir)if not U.editor then detail=not detail;U.redraw()end end
function on_exit()closed=true;cancel()end
