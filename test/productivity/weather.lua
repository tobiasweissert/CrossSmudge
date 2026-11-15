click('Refresh');on_update();assert(REQUEST);deliver(fixture('forecast.csv'));assert(TEXT:find('Updated',1,true))
click('Details');assert(TEXT:find('11-19',1,true));click('Overview');restart();assert(TEXT:find('Cached:',1,true))
click('Refresh');on_update();deliver(nil,'time_untrusted');assert(TEXT:find('Trust verified UTC',1,true))
click('Location');enter('Zurich');on_update();deliver(fixture('cities.json'));click('Zurich, CH');on_update();deliver(fixture('forecast.csv'))
click('Refresh');on_update();on_button('back',true);assert(CANCELLED);on_http{id=1,body='bad',status=200};draw();assert(TEXT:find('Cancelled',1,true))
local M=MODULES['model.lua'];assert(not M.forecast('{}'));assert(not M.forecast('{"current":null}'));assert(not M.cities('{"results":[{}]}'));assert(not M.location('99\t0\tx'))

assert(not M.forecast(fixture('forecast.csv')..fixture('forecast.csv')))
