assert(TEXT:find('25',1,true)==nil) -- large numbers are primitives
on_button('confirm',true);assert(AWAKE);NOW=60000;on_update();draw();click('Pause');assert(not AWAKE)
restart();local M=MODULES['core.lua'];local S=MODULES['storage.lua'];S.seq=0;local state=assert(S.load(M.decode));assert(state.remaining==1440 and not state.running)
click('Start');NOW=NOW+1440000;on_update();draw();assert(TEXT:find('Break',1,true)and not AWAKE)
click('Start');NOW=NOW+300000;on_update();draw();assert(TEXT:find('Session 2 of 4',1,true))
click('Reset');click('Focus');click('+');click('Return');restart()
local s={focus=25,short=5,long=15,phase=1,session=4,done=3,remaining=1,running=true,tick=2147483600,carry=0}
assert(M.tick(s,-2147482600));assert(s.phase==3 and s.remaining==900 and s.done==4)
M.next(s,false);assert(s.session==1 and s.phase==1)
assert(not M.decode('bad'))
-- Closing snapshots elapsed time since the last screen update, not Start time.
click('Start');NOW=NOW+17000;on_exit()
do local current=MODULES['storage.lua'];current.seq=0;local restored=assert(current.load(MODULES['core.lua'].decode));assert(restored.remaining==26*60-17)end
on_button('back',true);assert(EXITED)
