-- Sticky presentation adapter; the original controller owns rules and saves.
local app=dofile('cp_app.lua')
local V=dofile('board.lua')
local oldInit,oldTouch,oldButton=on_init,on_touch,on_button
local oldLayout=app.layout
local state=app.state()
local layout,answerPath,latest,view,year,month
local menuCursor=0
local months={'JAN','FEB','MAR','APR','MAY','JUN','JUL','AUG','SEP','OCT','NOV','DEC'}
local function action(id)
 local l=oldLayout()
 oldTouch(12+(id-1)%10*l.kw+2,l.ky+(id-1)//10*(l.kh+4)+2,'tap')
end
local function today()
 local d=smudge.get_date();if type(d)=='table'and d.year and d.month and d.day then return app.core.dayIndex(d.year,d.month,d.day)end;return latest or -1
end
local function available(day)
 if day<0 or day>8191 then return false end
 local word=smudge.read_file(answerPath,5,day*5)
 return word and #word==5 and word:match('^[A-Z]+$')~=nil
end
local function newest()return math.min(latest,today())end
local function result(day)return app.core.resultOf(state.results,day-app.resultOffset())&127 end
local function refresh()smudge.request_update()end
local function openDay(day)
 if app.begin(day,false)then view='game';state.menu=false end
 refresh()
end
local function showMonth(day)
 if day<0 then day=today()end
 year,month=app.core.dateOfDay(day);view='archive';state.menu=true
end
local function canStep(delta)
 local y,m=year,month+delta
 while m<1 do y=y-1;m=m+12 end
 while m>12 do y=y+1;m=m-12 end
 local first=app.core.dayIndex(y,m,1);local next=app.core.dayIndex(m==12 and y+1 or y,m==12 and 1 or m+1,1)
 return next>0 and first<=newest()
end
local function step(delta)
 if not canStep(delta)then return end
 month=month+delta
 while month<1 do year=year-1;month=month+12 end
 while month>12 do year=year+1;month=month-12 end
 refresh()
end
local function openMenu()state.menu=true;view='menu';menuCursor=0;refresh()end
local function menuAction(id)
 if id==0 then if newest()>=0 then openDay(newest())end
 elseif id==1 then view='help';refresh()
 elseif id==2 then if newest()>=0 then showMonth(newest());refresh()end
 elseif not state.menuModel.upToDate then view='import';refresh()end
end
function on_init()
 oldInit();V.init();local w,h=smudge.get_bounds();layout=V.layout(w,h);view='game'
 local localAnswers=smudge.read_file('wordle.ans',5,0);answerPath=localAnswers and #localAnswers==5 and 'wordle.ans'or'/.crosspoint/wordle.ans'
 -- Bounded positional probes preserve the controller's file-backed archive instead of retaining the bank in the heap.
 local lo,hi=0,8192
 while lo<hi do local mid=(lo+hi+1)//2;local chunk=smudge.read_file(answerPath,5,(mid-1)*5)
  if chunk and #chunk==5 then lo=mid else hi=mid-1 end
 end
 latest=lo-1
 while latest>=0 and not available(latest)do latest=latest-1 end
end
function on_draw()
 if state.closed then return end
 local w,h=smudge.get_bounds();layout=V.layout(w,h)
 if state.menu then
  if view=='game'then view='menu'end
  if view=='menu'then
   local day=newest();local date,status=nil,'NOT STARTED'
   if day>=0 then local y,m,d=app.core.dateOfDay(day);date=d..' '..months[m]..' '..y
    local r=result(day);if r==0 and state.day==day and state.game then r=8 end;if r>=1 and r<=6 then status='SOLVED IN '..r elseif r==7 then status='NOT SOLVED'elseif r==8 then status='IN PROGRESS'end
   end
   state.menuModel={day=day,date=date,status=status,stats=app.core.statsFor(state.results,today()-app.resultOffset()),puzzles=day+1,upToDate=latest>=today()}
   V.drawMenu(layout,state)
  elseif view=='help'then V.drawHelp(layout)
  elseif view=='archive'then
   local cells={};local first=app.core.dayIndex(year,month,1);local next=app.core.dayIndex(month==12 and year+1 or year,month==12 and 1 or month+1,1)
   local lead=(first%7+6)%7;local played=0
   for i=1,42 do cells[i]={day=0}end
   for index=first,next-1 do
    local i=lead+index-first+1;local exists=index<=newest()and available(index);local r=exists and result(index)or 0
    if exists and r==0 and state.day==index and state.game then r=8 end
    local finished=r>=1 and r<=7
    cells[i]={day=index-first+1,index=index,available=exists,today=index==today(),played=r~=0,finished=finished,lost=r==7}
    if finished then played=played+1 end
   end
   V.drawArchive(layout,{year=year,month=month,cells=cells,played=played,prevYear=canStep(-12),nextYear=canStep(12),prevMonth=canStep(-1),nextMonth=canStep(1)})
  else V.drawImport(layout,'Reopen Wordle after copying the files. Existing games and daily history are kept.')end
  layout.screen=view;return
 end
 view='game';layout.screen='game'
 if state.day>=0 then local _,m,d=app.core.dateOfDay(state.day);state.dateLabel=months[m]..' '..d else state.dateLabel='PRACTICE'end
 local nextAnswer=state.day>=0 and state.day<8191 and state.game.status~='Playing'and smudge.read_file(answerPath,5,(state.day+1)*5)
 state.canNext=type(nextAnswer)=='string'and #nextAnswer==5 and nextAnswer:match('^[A-Z]+$')~=nil
 V.draw(layout,state.game,state.notice,state,action)
end
local function inside(b,x,y)return b and x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h end
function on_touch(x,y,event)
 if state.closed or event~='tap'then return end
 if state.menu then
  if view=='help'or view=='import'then openMenu();return end
  if view=='archive'then
   if y<83 then openMenu();return end
   local c=layout.calendar
   for _,pair in ipairs({{c.prevYear,-12},{c.nextYear,12},{c.prevMonth,-1},{c.nextMonth,1}})do if inside(pair[1],x,y)then step(pair[2]);return end end
   if inside(c.today,x,y)then showMonth(newest());refresh();return end
   for _,b in ipairs(c.cells)do if b.available and inside(b,x,y)then openDay(b.index);return end end
  else
   if inside(layout.hero,x,y)then menuAction(0);return end
   for id,b in ipairs(layout.menu or{})do if inside(b,x,y)then if b.enabled then menuAction(id)end;return end end
  end
 else
  V.touch(layout,x,y,false,action,function()
   if state.game.status~='Playing'and y>=layout.result[1].y then showMonth(state.day);refresh()else openMenu()end
  end,function()action(30);state.menuIndex=4;oldButton('confirm',true)end)
 end
end
function on_button(button,pressed)
 if not pressed or state.closed then return end
 if state.menu then
  if button=='back'then
   if view~='menu'then openMenu()else state.menuIndex=7;oldButton('confirm',true)end
  elseif view=='menu'then
   if button=='up'or button=='left'or button=='page_back'then menuCursor=(menuCursor+3)%4
   elseif button=='down'or button=='right'or button=='page_forward'then menuCursor=(menuCursor+1)%4
   elseif button=='confirm'then menuAction(menuCursor)end
  elseif view=='archive'then
   if button=='left'or button=='page_back'then step(-1)elseif button=='right'or button=='page_forward'then step(1)elseif button=='confirm'then showMonth(newest());refresh()end
  elseif button=='confirm'then openMenu()end
 else oldButton(button,pressed);if state.menu then view='menu';menuCursor=0 end end
end
app.layout=function()return layout end
return app
