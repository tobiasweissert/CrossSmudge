local function num(v,lo,hi) return type(v)=='number' and v==v and v>=lo and v<=hi end
local function stamp(s) return type(s)=='string' and s:match('^%d%d%d%d%-%d%d%-%d%dT%d%d:%d%d$')~=nil end
-- Open-Meteo's numeric CSV avoids constructing an unused JSON object tree.
-- This is a parser for this fixed API schema, not arbitrary user CSV.
return function(raw)
 if type(raw)~='string' or #raw>3800 or raw:find('%z')then return end
 local c,h,b={},{time={},temperature_2m={},weather_code={}},{time={},weather_code={},temperature_2m_max={},temperature_2m_min={}}
 local section,row=0,0
 for line in (raw:gsub('\r','')..'\n'):gmatch('(.-)\n')do
  if line==''then
  elseif line:match('^latitude,longitude,')then if section~=0 then return end;section=1;row=0
  elseif line=='time,temperature_2m (°C),apparent_temperature (°C),weather_code (wmo code),wind_speed_10m (km/h)'then if section~=1 or row~=1 then return end;section=2;row=0
  elseif line=='time,temperature_2m (°C),weather_code (wmo code)'then if section~=2 or row~=1 then return end;section=3;row=0
  elseif line=='time,weather_code (wmo code),temperature_2m_max (°C),temperature_2m_min (°C)'then if section~=3 or row~=16 then return end;section=4;row=0
  else
   row=row+1
   if section==1 then if row~=1 then return end
   elseif section==2 then
    if row~=1 then return end
    local time,t,a,code,wind=line:match('^([^,]+),([^,]+),([^,]+),([^,]+),([^,]+)$')
    t,a,code,wind=tonumber(t),tonumber(a),tonumber(code),tonumber(wind)
    if not stamp(time)or not num(t,-100,70)or not num(a,-120,90)or not num(code,0,99)or not num(wind,0,500)then return end
    c={time=time,temperature_2m=t,apparent_temperature=a,weather_code=code,wind_speed_10m=wind}
   elseif section==3 then
    if row>16 then return end
    local time,t,code=line:match('^([^,]+),([^,]+),([^,]+)$');t,code=tonumber(t),tonumber(code)
    if not stamp(time)or not num(t,-100,70)or not num(code,0,99)then return end
    h.time[row]=time;h.temperature_2m[row]=t;h.weather_code[row]=code
   elseif section==4 then
    if row>5 then return end
    local time,code,hi,lo=line:match('^([^,]+),([^,]+),([^,]+),([^,]+)$');code,hi,lo=tonumber(code),tonumber(hi),tonumber(lo)
    if not time or not time:match('^%d%d%d%d%-%d%d%-%d%d$')or not num(code,0,99)or not num(hi,-100,70)or not num(lo,-100,70)then return end
    b.time[row]=time;b.weather_code[row]=code;b.temperature_2m_max[row]=hi;b.temperature_2m_min[row]=lo
   else return end
  end
 end
 if c.time and #h.time==16 and #b.time==5 then return{current=c,hourly=h,daily=b}end
end
