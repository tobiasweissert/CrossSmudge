local M,g=test_core,test_game
input_frame('down',30,110,0);input_frame('tick',0,0,499);assert(g.b[1]==0)
local handled,suppressed=input_frame('tick',0,0,500);assert(handled and suppressed and g.b[1]==4)
input_frame('tick',0,0,600);input_frame('tick',0,0,900);assert(g.b[1]==4)
input_frame('release',0,0,1000);assert(g.b[1]==4 and g.status==0,'hold release dug')
input_frame('down',30,110,2000);input_frame('tick',0,0,2500);input_frame('release',0,0,2600);assert(g.b[1]==0 and g.status==0)
for _,kind in ipairs({'move','roll','cancel','multi'})do
 input_frame('down',30,110,3000);input_frame(kind,0,0,3100);input_frame('tick',0,0,3600);input_frame('release',0,0,3700);assert(g.b[1]==0 and g.status==0,kind)
end
input_frame('down',30,110,4000);input_frame('disabled',0,0,4600);input_frame('cancel',0,0,4700);input_frame('release',0,0,4800);assert(g.b[1]==0)
-- A short release digs exactly through the production callback.
input_frame('down',30,110,5000);input_frame('release',0,0,5100);assert(g.b[1]&2~=0 and g.status==1)
local before=M.encode(g)
input_frame('down',30,110,6000);input_frame('tick',0,0,6600);input_frame('release',0,0,6700);assert(before==M.encode(g))
-- No callback => no consuming query; a long-contact release still reaches legacy tap.
on_long_press=nil;local taps=0;on_touch=function(x,y,event)assert(event=='tap');taps=taps+1 end
input_frame('down',100,100,7000);local _,consumed=input_frame('tick',0,0,7600);assert(not consumed)
input_frame('release',0,0,7700);assert(taps==1)
print('real input: threshold, one-shot, release suppression, unflag, movement, cancellation, multi, disabled, short dig, revealed no-op, legacy tap PASS')
-- An opt-in private hold hides once on lift, multi-contact, disable or exit.
local visible,reveals,hides,extraTaps=false,0,0,0
on_long_press=function(x,y)assert(x==100 and y==100);visible=true;reveals=reveals+1 end
on_touch_release=function(x,y)assert(x==100 and y==100);visible=false;hides=hides+1 end
on_touch=function()extraTaps=extraTaps+1 end
input_frame('down',100,100,8000);input_frame('tick',0,0,8500);assert(visible)
input_frame('tick',0,0,8600);assert(visible and reveals==1)
input_frame('release',0,0,8700);assert(not visible and hides==1 and extraTaps==0)
input_frame('tick',0,0,8800);assert(hides==1)
input_frame('down',100,100,9000);input_frame('tick',0,0,9500);assert(visible)
input_frame('multi',0,0,9600);assert(not visible and hides==2)
input_frame('release',0,0,9700);assert(hides==2 and extraTaps==0)
input_frame('down',100,100,10000);input_frame('tick',0,0,10500);assert(visible)
input_frame('disabled',0,0,10600);assert(not visible and hides==3)
input_frame('release',0,0,10700);assert(hides==3)
input_frame('down',100,100,11000);input_frame('tick',0,0,11500);assert(visible)
input_frame('exit',0,0,11600);assert(not visible and hides==4)
input_frame('release',0,0,11700);assert(hides==4 and extraTaps==0)
print('private hold: one-shot reveal/hide, release, multi-contact, disabled, exit and no release tap PASS')
