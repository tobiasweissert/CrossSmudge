local function decode(raw)
 collectgarbage('collect')
 local result=dofile('json.lua').decode(raw)
 collectgarbage('collect')
 return result
end
local M={}
local function num(v,lo,hi) return type(v)=='number' and v==v and v>=lo and v<=hi end
function M.condition(c)
 if c==0 then return 'Clear' elseif c<=3 then return 'Cloudy' elseif c<=48 then return 'Fog'
 elseif c<=57 then return 'Drizzle' elseif c<=67 then return 'Rain' elseif c<=77 then return 'Snow'
 elseif c<=82 then return 'Rain showers' elseif c<=86 then return 'Snow showers' else return 'Thunderstorm'end
end
function M.forecast(raw)
 collectgarbage('collect')
 return dofile('forecast.lua')(raw)
end
function M.location(raw)
 if type(raw)~='string'then return end
 local lat,lon,name=raw:match('^([%d%.%-]+)\t([%d%.%-]+)\t([^\n\t]+)$')
 lat,lon=tonumber(lat),tonumber(lon)
 if not num(lat,-90,90) or not num(lon,-180,180)or not name or #name>80 or name:find('[%z\1-\31]')then return end
 return {lat=lat,lon=lon,name=name}
end
function M.encodeLocation(l)return string.format('%.4f\t%.4f\t%s',l.lat,l.lon,l.name)end
function M.cities(raw)
 local d=decode(raw);collectgarbage('collect');if type(d)~='table'then return end
 if d.results==nil then return {}end
 if type(d.results)~='table' or #d.results>3 then return end
 local cities={}
 for _,v in ipairs(d.results)do
  if type(v)~='table' or type(v.name)~='string' or #v.name>48 or v.name:find('[%z\1-\31]') or not num(v.latitude,-90,90) or not num(v.longitude,-180,180)then return end
  local country=type(v.country_code)=='string' and v.country_code:match('^%u%u$')and v.country_code or ''
  cities[#cities+1]={name=v.name..(country~='' and ', '..country or ''),lat=v.latitude,lon=v.longitude}
 end
 return cities
end
function M.cache(raw)
 if type(raw)~='string'then return end
 local loc,body=raw:match('^([^\n]+)\n(.*)$');local l=M.location(loc);local f=body~=''and M.forecast(body)or nil
 if l and (body==''or f) then return {location=l,forecast=f}end
end
return M
