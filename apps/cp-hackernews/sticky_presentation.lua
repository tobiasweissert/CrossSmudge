-- Pinned HackerNewsScreens Notice/List/Reader geometry and real original cuts.
-- Reader/controller storage, requests, retries and comment cursors stay untouched.
local F=dofile('original_graphics.lua')
local V={page=1,pages=1,hit={},slice=0,feedMenu=false,menuFocus=1,visibleLines={}}
local current,key,pendingSlice
local feeds={'TOP','NEW','ASK','SHOW'}
local function box(x,y,w,h)return{x=x,y=y,w=w,h=h}end
local function hit(kind,id,b)V.hit[#V.hit+1]={kind=kind,id=id,x=b.x,y=b.y,w=b.w,h=b.h}end
local function request()smudge.request_update()end
local function stroke(b,n)smudge.rect(b.x,b.y,b.w,b.h,false,n or 1,true)end
local function button(label,b,kind,id,filled,enabled,radius)
 if enabled==false then smudge.rect_dither(b.x,b.y,b.w,b.h,true)
 elseif radius then smudge.rounded_rect(b.x,b.y,b.w,b.h,radius,filled,filled,1)
 else smudge.rect(b.x,b.y,b.w,b.h,true,filled==true);if not filled then stroke(b)end end
 F.lineBox(label,box(b.x+4,b.y+2,b.w-8,b.h-4),'body',filled or enabled==false,'center')
 if kind and enabled~=false then hit(kind,id,b)end
end
-- ToyboxText::fitLines: preserve word boundaries and mark discarded text.
local function fitLines(text,width,lines,key)
 text=F.visible(text,key)
 local line,lineStart,consumed,lastStart,number,i='',1,0,1,1,1
 local overflow=false
 while i<=#text+1 do
  local space=text:find(' ',i,true)or #text+1;local word=text:sub(i,space-1)
  local candidate=line==''and word or line..' '..word
  if F.width(candidate,key)<=width then line=candidate;consumed=space-1;lastStart=lineStart
  elseif number<lines then
   number=number+1;lineStart=i;line=word
   if F.width(word,key)<=width then consumed=space-1;lastStart=lineStart end
  else overflow=true;break end
  if space>#text then break end;i=space+1
 end
 if not overflow and consumed>=#text then return text end
 if consumed==0 then
  local cut=text
  while #cut>0 and F.width(cut..'...',key)>width do local at=utf8.offset(cut,-1);cut=cut:sub(1,at-1)end
  return cut==''and''or cut..'...'
 end
 local kept=text:sub(1,consumed):gsub(' +$','')
 if lastStart>#kept+1 then lastStart=1 end
 while #kept>=lastStart and F.width(kept:sub(lastStart)..'...',key)>width do
  local space=kept:match('.*() ');if not space or space<lastStart then break end
  kept=kept:sub(1,space-1)
 end
 return kept..'...'
end
local function fitted(text,width,key)return fitLines(text,width,1,key)end
local function header(title,pageLabel,reading)
 local w=smudge.get_bounds();smudge.rect(0,0,w,76,true,true);smudge.rect(0,80,w,3,true,true)
 local font=reading and'readerTitle'or'title';local available=w-32
 local pageWidth=pageLabel and F.width(pageLabel,'body')or 0
 if pageLabel then available=available-pageWidth-6 end
 if reading then
  for _,candidate in ipairs({'readerTitle','body','readerSmall'})do font=candidate;if F.width(F.visible(title,candidate),candidate)<=available then break end end
 end
 F.lineBox(fitted(title,available,font),box(16,0,available,76),font,true)
 if pageLabel then
  local height=F.metrics[font][1];local y=(76-height)//2+height-40
  F.lineBox(pageLabel,box(w-16-pageWidth,y,pageWidth,40),'body',true,'right')
 end
end
local function bodyBox(w,h)return box(16,119,w-32,h-199)end
local function centerLines(lines,b,font)
 local lh=F.metrics[font][1];local y=b.y+math.max(0,(b.h-#lines*lh)//2)
 for _,line in ipairs(lines)do F.lineBox(line,box(b.x,y,b.w,lh),font,false,'center');y=y+lh end
end
local function notice(headline,message,kind,label)
 local w,h=smudge.get_bounds();V.hit={};smudge.clear();header('HACKER NEWS')
 local lines=F.wrap(headline,w-32,'title');while #lines>2 do table.remove(lines)end
 local y=119;local top=y+math.max(0,(126-#lines*63)//2)
 for _,line in ipairs(lines)do F.text(line,16,top,'title');top=top+63 end
 y=y+126+12;smudge.rect(16,y,w-32,3,true,true);y=y+27
 local capacity=math.max(0,(h-16-(label and 64 or 0)-y)//40);local count=0
 F.lines(message or'',w-32,'body',function(line)count=count+1;if count<=capacity then F.text(line,16,y+(count-1)*40,'body')end end)
 if label then button(label,box(16,h-68,w-32,52),kind,nil,true)end
end
local function listFooter(s,w,h)
 local half=(w-44)//2
 button(feeds[s.tab],box(16,h-68,half,52),nil,nil,true)
 button('FEEDS',box(16+half+12,h-68,half,52),'feeds',nil,false)
end
function V.init()F.init()end
function V.list(s)
 current=s;V.hit={};V.visibleLines={};local w,h=smudge.get_bounds()
 local newKey=s.tab..':'..s.listPage
 if key~=newKey then V.slice=pendingSlice or 0;pendingSlice=nil;key=newKey end
 if #s.ids==0 and s.loading and not V.feedMenu then notice('FETCHING THE FRONT PAGE','');return end
 smudge.clear();header('HACKER NEWS');listFooter(s,w,h)
 local b=bodyBox(w,h);local rowHeight=V.feedMenu and 62 or 92
 V.capacity=math.max(1,(b.h+4)//(rowHeight+4))
 if V.feedMenu then
  for index,name in ipairs(feeds)do
   local row=box(16,119+(index-1)*66,w-32,62)
   local selected=index==s.tab
   smudge.rect(row.x,row.y,row.w,row.h,true,selected);if not selected then stroke(row)end
   F.lineBox(name,box(row.x+12,row.y,row.w-24,row.h),'body',selected)
   if V.menuKeyboard and index==V.menuFocus then smudge.rect(row.x+3,row.y+3,row.w-6,row.h-6,false,3,not selected)end
   hit('tab',index,row)
  end
  return
 end
 if #s.ids==0 then
  local headline=s.error~=''and'NO LUCK'or'NOT LOADED YET'
  local message=s.error~=''and s.error or'The current feed needs a connection.'
  local head=F.wrap(headline,b.w,'title');local lines=F.wrap(message,b.w,'body')
  local height=#head*63+12+#lines*40+76;local y=b.y+(b.h-height)//2
  centerLines(head,box(b.x,y,b.w,#head*63),'title');y=y+#head*63+12
  centerLines(lines,box(b.x,y,b.w,#lines*40),'body');y=y+#lines*40+24
  local bw=b.w*3//4;button(s.error~=''and'TRY AGAIN'or'LOAD',box(b.x+(b.w-bw)//2,y,bw,52),'refresh',nil,true,true,20)
  return
 end
 local count=math.min(6,#s.ids-(s.listPage-1)*6);local slices=math.max(1,(count+V.capacity-1)//V.capacity)
 V.slice=math.max(0,math.min(V.slice,slices-1));V.slices=slices
 local first=V.slice*V.capacity+1;local shown=math.min(V.capacity,count-first+1)
 local rowWidth=b.w-(count>V.capacity and 5 or 0)
 for row=1,shown do
  local index=first+row-1;local item=s.items[index];local rb=box(16,119+(row-1)*96,rowWidth,92)
  smudge.rect(rb.x,rb.y,rb.w,rb.h,true,false);stroke(rb)
  if item then
   local value=tostring(item.descendants or 0);local valueWidth=F.width(value,'small')
   local labelWidth=rb.w-24-valueWidth-10
   local originalWidth=b.w-24-F.width('8888','small')-12
   local title=fitLines(item.title,originalWidth,2,'body')
   local lines=F.wrap(title,labelWidth,'body')
   if #lines>2 then lines[2]=fitLines(lines[2],labelWidth,1,'body');while #lines>2 do table.remove(lines)end end
   local y=rb.y+math.max(0,(rb.h-#lines*40)//2)
   for _,line in ipairs(lines)do F.text(line,rb.x+12,y,'body');y=y+40 end
   F.lineBox(value,box(rb.x+rb.w-12-valueWidth,rb.y,valueWidth,rb.h),'small',false,'right')
   -- The original count occupies this trailing column; its existing target
   -- opens comments, while the source-shaped title row opens the article.
   hit('comments',index,box(rb.x+rb.w-72,rb.y,72,rb.h));hit('story',index,box(rb.x,rb.y,rb.w-72,rb.h))
  else
   local label=s.failed[index]and'Unavailable - tap to retry'or s.loading and'Loading...'or'Tap to retry'
   F.lineBox(label,box(rb.x+12,rb.y,rb.w-24,rb.h),'body');hit('retry',index,rb)
  end
 end
 if count>V.capacity then
  smudge.rect_dither(w-19,119,3,b.h,false);local thumb=math.max(12,b.h*shown//count);local y=119+(b.h-thumb)*(first-1)//(count-shown)
  smudge.rect(w-19,y,3,thumb,true,true)
 end
end
local function walk(s,width,visit)
 if s.screen=='article'then F.lines(s.articleText or'',width,'body',visit);return end
 if #s.comments==0 and(s.selfText or'')==''then F.lines(s.selected.url and'No comments on this one yet.\n'or'No comments yet.\n',width,'body',visit);return end
 F.lines(s.selected.title,width,'body',visit);visit('')
 for paragraph in (s.selfText or''):gmatch('[^\n]+')do F.lines(paragraph,width,'body',visit);visit('')end
 for _,comment in ipairs(s.comments)do
  local author=string.rep('>',comment.depth)..(comment.depth>0 and' 'or'')..string.upper(comment.by)
  F.lines(author,width,'body',visit);visit('')
  for paragraph in comment.text:gmatch('[^\n]+')do F.lines(paragraph,width,'body',visit);visit('')end
 end
end
function V.thread(s)
 current=s;V.hit={};V.visibleLines={};local w,h=smudge.get_bounds();local article=s.screen=='article'
 if s.loading and (article and(s.articleText or'')==''or not article and #s.comments==0 and(s.selfText or'')=='')then notice(article and'FETCHING THE ARTICLE'or'FETCHING THE COMMENTS','');return end
 if s.error~=''and(article and(s.articleText or'')==''or not article and #s.comments==0 and(s.selfText or'')=='')then
  notice('NO LUCK',s.error,'back','BACK TO THE LIST');return
 end
 local body=bodyBox(w,h);local count=0;walk(s,body.w,function()count=count+1 end)
 local capacity=math.max(1,body.h//40);V.pages=math.max(1,(count+capacity-1)//capacity);V.page=math.max(1,math.min(V.page,V.pages))
 smudge.clear();header(s.selected.title,V.page..' / '..V.pages,true)
 local first=(V.page-1)*capacity+1;local index=0
 walk(s,body.w,function(line)
  index=index+1;if index<first or index>=first+capacity then return end
  V.visibleLines[#V.visibleLines+1]=line;F.text(line,body.x,body.y+(index-first)*40,'body')
 end)
 local usable=w-32;local arrow=(usable-24)//4;local middle=usable-2*arrow-24
 button('<',box(16,h-68,arrow,52),'previous',nil,true,V.page>1)
 local selectedRow;for row,item in pairs(s.items)do if item.id==s.selected.id then selectedRow=row;break end end
 local canSwap=article and selectedRow~=nil or not article and(s.selected.url~=nil or(s.selfText or'')~='')
 button(article and'COMMENTS'or'ARTICLE',box(16+arrow+12,h-68,middle,52),article and'comments'or'article',selectedRow,true,canSwap)
 button('>',box(16+arrow+middle+24,h-68,arrow,52),'next',nil,true,V.page<V.pages or not article and s.more)
 if s.error~=''or not article and s.more then
  local label=s.error~=''and s.error or'More comments available'
  local text=fitted(label,w-32,'readerSmall');F.lineBox(text,box(16,84,w-32,31),'readerSmall')
 end
end
function V.navigate(delta)
 if not current or current.screen~='list'or V.feedMenu or #current.ids==0 then return false end
 local nextSlice=V.slice+delta
 if nextSlice>=0 and nextSlice<(V.slices or 1)then V.slice=nextSlice;request();return true end
 if delta<0 and current.listPage==1 or delta>0 and current.listPage*6>=#current.ids then return true end
 pendingSlice=delta>0 and 0 or math.max(0,(6+(V.capacity or 6)-1)//(V.capacity or 6)-1)
 return false
end
function V.beforeButton(button)
 if V.feedMenu then
  if button=='back'then V.feedMenu=false;request();return true end
  if button=='confirm'then for _,b in ipairs(V.hit)do if b.kind=='tab'and b.id==V.menuFocus then on_touch(b.x+b.w//2,b.y+b.h//2,'tap');return true end end
  else V.menuKeyboard=true;V.menuFocus=(V.menuFocus-1+((button=='up'or button=='left'or button=='page_back')and -1 or 1))%4+1;request();return true end
 end
 if current and current.screen=='list'then
  if button=='down'or button=='right'or button=='page_forward'then return V.navigate(1)
  elseif button=='up'or button=='left'or button=='page_back'then return V.navigate(-1)
  elseif button=='confirm'and V.slice>0 then for _,b in ipairs(V.hit)do if b.kind=='story'then on_touch(b.x+b.w//2,b.y+b.h//2,'tap');return true end end end
 end
 return false
end
function V.at(x,y)
 for _,b in ipairs(V.hit)do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then
  if b.kind=='feeds'then V.feedMenu=not V.feedMenu;V.menuKeyboard=false;V.menuFocus=current.tab;request();return end
  if b.kind=='tab'then V.feedMenu=false;V.slice=0;key=nil end
  return b.kind,b.id
 end end
end
return V
