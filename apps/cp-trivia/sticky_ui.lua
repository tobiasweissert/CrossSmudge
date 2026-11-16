-- TriviaScreens.cpp Notice/Choice composition, with explicit paging for the
-- existing provider's text (the original pack pre-fits its questions/options).
local F=dofile('graphics.lua')
local U={hit={},page=1,pages=1,detail=nil,focus=1}
local function box(x,y,w,h)return{x=x,y=y,w=w,h=h}end
local function hit(b,kind,value)U.hit[#U.hit+1]={box=b,kind=kind,value=value}end
local function outline(b,weight)smudge.rect(b.x,b.y,b.w,b.h,false,weight or 1,true)end
local function button(text,b,kind,value,filled)
 if filled then smudge.rect(b.x,b.y,b.w,b.h,true,true)else outline(b,2)end
 F.label(text,b,'small',filled);hit(b,kind,value)
end
local function header(w,right)
 smudge.rect(0,0,w,76,true,true);F.label('TRIVIA',box(16,0,w-32,76),'title',true,'left')
 if right then
  local label=F.visible(right,'small')
  while F.width(label,'small')>w-208 and #label>0 do local at=utf8.offset(label,-1);label=label:sub(1,at-1)end
  F.label(label,box(192,0,w-208,76),'small',true,'right')
 end
 smudge.rect(0,80,w,3,true,true)
end
local function footer(w,h,primary,action,readQuestion)
 local full=w-32;local aside=box(w-148,h-80,132,64)
 if primary then
  if readQuestion then
   -- Original three-action footer geometry, used only when unrestricted
   -- provider text needs the supported complete-reading surface.
   local wide=full-288
   button(primary=='SKIP'and'READ'or primary,box(16,h-80,wide,64),primary=='SKIP'and'question-detail'or action,nil,true)
   button(primary=='SKIP'and'SKIP'or'READ',box(16+wide+12,h-80,132,64),primary=='SKIP'and action or'question-detail',nil,false)
  else button(primary,box(16,h-80,full-144,64),action,nil,true)end
 end
 button('END',aside,'exit',nil,false)
end
local function pageText(text,b,key,center)
 local lines=F.wrap(text,b.w,key);local capacity=math.max(1,b.h//F.metrics[key][1])
 U.pages=math.max(1,(#lines+capacity-1)//capacity);U.page=math.max(1,math.min(U.page,U.pages))
 local visible={};for i=(U.page-1)*capacity+1,math.min(#lines,U.page*capacity)do visible[#visible+1]=lines[i]end
 F.prose(visible,b,key,nil,center)
 if U.pages>1 then
  F.label(U.page..'/'..U.pages,box(b.x,b.y+b.h,b.w,29),'small');hit(box(b.x,b.y,b.w//2,b.h),'page',-1);hit(box(b.x+b.w//2,b.y,b.w-b.w//2,b.h),'page',1)
 end
 U.visibleLines=visible;U.totalLines=#lines
end
function U.init()F.init();U.page=1;U.detail=nil;U.focus=1 end
function U.reset()U.page=1;U.detail=nil;U.focus=1 end
function U.draw(s)
 local w,h=smudge.get_bounds();smudge.clear();header(w,s.q and w>h and s.status or nil);U.hit={};U.visibleLines={}
 if not s.q then
  local top=h>=600 and 179 or 103
  F.label((s.request or s.queued) and 'FETCHING' or 'NO QUESTIONS',box(16,top,w-32,64),'title')
  local body=s.status..'\n\nOpen Trivia DB / CC BY-SA 4.0. Fetch one live question; no bundled questions.'
  pageText(body,box(16,top+88,w-32,math.min(260,math.max(40,h-112-top-88-29))),'body',true)
  button((s.request or s.queued) and 'FETCHING' or 'FETCH QUESTION',box(16,h-80,w-32,64),'fetch',nil,true)
 elseif U.detail then
  pageText(U.detail==0 and s.q.question or s.q.question..'\n\n'..s.q.answers[U.detail],box(16,117,w-32,math.max(40,h-117-112-29)),'body')
  footer(w,h,(U.detail==0 or s.answered) and 'BACK' or 'CHOOSE',(U.detail==0 or s.answered) and 'close-detail' or 'choose-detail')
 else
  -- No fabricated difficulty, pack totals, scores or records: this provider
  -- interface exposes only the current question and its checked answer.
  local status=F.visible(s.status,'small')
  while F.width(status,'small')>w-32 and #status>0 do local at=utf8.offset(status,-1);status=status:sub(1,at-1)end
  if w<=h then F.text(status,16,94,'small')end
  local row=h>=600 and 62 or 48;local step=row+8;local optionsTop=h-96-step*4
  local top=h>=600 and 137 or 101;local clueHeight=math.max(40,optionsTop-top-12)
  local clueLines=F.wrap(s.q.question,w-32,'body');local capacity=math.max(1,clueHeight//40)
  if #clueLines>capacity then
   local visible={};for i=1,capacity do visible[i]=clueLines[i]end
   F.prose(visible,box(16,top,w-32,clueHeight),'body');hit(box(16,top,w-32,clueHeight),'question-detail')
   -- The existing provider has unrestricted question lengths; the original
   -- pack pre-fits them. The clue itself opens its complete paged text.
   U.questionLong=true;U.visibleLines=visible;U.totalLines=#clueLines;U.pages=1
  else U.questionLong=false;pageText(s.q.question,box(16,top,w-32,clueHeight),'body')end
  U.long={}
  for i,answer in ipairs(s.q.answers)do
   local b=box(16,optionsTop+(i-1)*step,w-32,row);local correct=s.answered and answer==s.q.correct;local wrong=s.answered and i==s.choice and not correct
   if correct then smudge.rect(b.x,b.y,b.w,b.h,true,true)else outline(b,wrong and 3 or 1)end
   local label=F.visible(answer,'small');local fits=F.width(label,'small')<=b.w-32;U.long[i]=not fits
   if not fits then
    while #label>0 and F.width(label..'...','small')>b.w-32 do local at=utf8.offset(label,-1);label=label:sub(1,at-1)end
    label=label..'...'
   end
   F.label(label,b,'small',correct)
   if s.answered and i==s.choice then smudge.rect(b.x+5,b.y+5,7,b.h-10,true,not correct)
   elseif not s.answered and i==s.choice and U.keyboard then outline(box(b.x+3,b.y+3,b.w-6,b.h-6),2)end
   hit(b,s.answered and (not fits and 'detail' or 'none') or not fits and 'detail' or 'choose',i)
  end
  footer(w,h,s.answered and 'NEXT' or 'SKIP','fetch',U.questionLong)
  if U.keyboard and not s.answered and s.choice==5 then
   for _,target in ipairs(U.hit)do if target.kind=='fetch'then local b=target.box
    smudge.rect(b.x+4,b.y+4,b.w-8,b.h-8,false,2,U.questionLong)
   end end
  end
 end
end
function U.touch(x,y)
 for _,target in ipairs(U.hit)do local b=target.box
  if x>=b.x and y>=b.y and x<b.x+b.w and y<b.y+b.h then return target.kind,target.value end
 end
end
function U.turn(delta)U.page=math.max(1,math.min(U.pages,U.page+delta));smudge.request_update()end
return U
