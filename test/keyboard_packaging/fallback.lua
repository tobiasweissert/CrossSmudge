-- Real packaged helper execution with absent old-firmware keyboard APIs.
-- Drawing calls are controlled spies; this is availability/dispatch, not visual qualification.
local calls={}
smudge={get_device=function()return 'sticky'end}
for _,name in ipairs({'rect','text','rect_dither'})do
 smudge[name]=function(...)calls[#calls+1]={name,...}end
end
smudge.line_height=function()return 16 end
local K=assert(loadfile(folder..'/'..helper))()
if helper=='keyboard.lua' then
 local action=function()end
 local U={buttons={},focus=1,editor={value='retained'}}
 U.button=function(...)return 'nonsticky-delegated'end
 assert(K.editor(U)==false,'old firmware must not require keyboard_snapshot')
 K.button(U,'A',10,20,30,40,action,false,'A')
 assert(#calls==3 and calls[1][1]=='rect' and calls[3][1]=='text')
 assert(#U.buttons==1 and U.buttons[1][5]==action and U.buttons[1][7]=='A')
 smudge.get_device=function()return 'other'end
 assert(K.button(U,'B',10,20,30,40,action)== 'nonsticky-delegated')
 smudge.get_device=nil
 assert(K.editor(U)==false)
 assert(K.button(U,'B',10,20,30,40,action)== 'nonsticky-delegated')
else
 local b={x=10,y=20,w=30,h=40}
 assert(K.paint('1',b,true)==false,'old firmware must return to caller-owned paint')
 assert(#calls==0)
 smudge.keyboard_key=function(...)calls[#calls+1]={'keyboard_key',...}end
 assert(K.paint('1',b,true)==true and #calls==1)
 smudge.get_device=function()return 'other'end
 assert(K.paint('1',b,true)==false)
 smudge.get_device=nil
 assert(K.paint('1',b,true)==false)
end
