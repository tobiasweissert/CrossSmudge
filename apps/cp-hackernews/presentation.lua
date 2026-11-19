-- Sticky selects the original native presentation; other devices retain this exact legacy body.
if smudge.get_device and smudge.get_device()=='sticky'then return dofile('sticky_presentation.lua')end
local U=dofile('design.lua');local V={page=1,pages=1,hit={}}
function V.init()U.init()end
local function clean(s)return (tostring(s or ''):gsub('’',"'"):gsub('“','"'):gsub('”','"'):gsub('—','-'):gsub('–','-'))end
local function age(epoch)
 if type(epoch)~='number' then return '' end
 local seconds=math.max(0,smudge.time()-epoch)
 return seconds<3600 and math.max(1,seconds//60)..'m ago' or seconds<86400 and seconds//3600 ..'h ago' or seconds//86400 ..'d ago'
end
local function hit(kind,id,x,y,w,h)V.hit[#V.hit+1]={kind=kind,id=id,x=x,y=y,w=w,h=h}end
function V.at(x,y)for _,b in ipairs(V.hit) do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then return b.kind,b.id end end end
function V.list(s)
 local w,h=smudge.get_bounds();V.hit={};smudge.clear();U.header('Hacker News',s.updated and 'Updated '..s.updated or '',w)
 for i,title in ipairs({'Top','New','Ask','Show'}) do local x=(i-1)*w//4
  U.tile(x,65,w//4,54,i==s.tab and 'black' or 'white','black');U.text(title,x+w//8,92,'b28','center',i==s.tab);hit('tab',i,x,65,w//4,54)
 end
 local rh=(h-164)//6
 for row=1,6 do local index=(s.listPage-1)*6+row;if index>#s.ids then break end
  local y=120+(row-1)*rh;local item=s.items[row];U.text(index,42,y+rh//2,'b28','right',false,true)
  if item then
   local font=rh<80 and 'b20' or 'b28'
   for n,line in ipairs(U.wrap(clean(item.title),w-170,font,rh<80 and 1 or 3)) do U.text(line,64,y+14+(n-1)*27,font) end
   U.text((item.score or 0)..' points · '..age(item.time),64,y+rh-14,'r20',nil,false,true)
   U.round(w-78,y+rh//2-25,60,50,12,false,false);U.text(item.descendants or 0,w-48,y+rh//2,'b20','center');hit('comments',row,w-78,y+rh//2-25,60,50);hit('story',row,0,y,w,rh)
  else
   local failed=s.failed and s.failed[row];U.text(failed and 'Unavailable - tap to retry' or s.loading and 'Loading...' or 'Tap to retry',64,y+rh//2,'r20');hit('retry',row,0,y,w,rh)
  end
  smudge.rect_dither(0,y+rh-1,w,1,true)
 end
 local pages=math.max(1,math.ceil(#s.ids/6));local footer=s.error~='' and s.error or ('Page '..s.listPage..' of '..pages)
 U.text(U.wrap(clean(footer),w-32,'r20',1)[1] or '',w//2,h-20,'r20','center',false,true)
 hit('previous',nil,0,h-44,w//2,44);hit('next',nil,w//2,h-44,w-w//2,44)
 if #s.ids==0 then U.text(s.error~='' and s.error or 'Loading stories',w//2,h//2,'r20','center');hit('refresh',nil,16,140,w-32,h-200) end
end
function V.thread(s)
 local w,h=smudge.get_bounds();V.hit={};smudge.clear();local article=s.screen=='article';local item=s.selected
 U.text('<',24,32,'b28');hit('back',nil,0,0,64,64);U.text(article and 'Article' or (item.descendants or 0)..' comments',64,32,'b28')
 local count=0
 local visit
 local function blank(depth)visit('',depth,false)end
 local function walk()
  local function emit(text,depth,bold)
   U.lines(clean(text),w-48-depth*20,bold and 'b20' or 'r20',function(line)visit(line,depth,bold)end)
  end
  if article then
   emit(item.title,0,true);blank(0)
   for paragraph in (s.articleText or ''):gmatch('[^\n]+') do emit(paragraph,0,false);blank(0) end
  else
   for paragraph in (s.selfText or ''):gmatch('[^\n]+') do emit(paragraph,0,false) end
   if s.selfText and s.selfText~='' then blank(0) end
   for _,comment in ipairs(s.comments) do
    emit(comment.by..' · '..age(comment.time),comment.depth,true)
    for paragraph in comment.text:gmatch('[^\n]+') do emit(paragraph,comment.depth,false) end
    blank(comment.depth)
   end
  end
 end
 visit=function()count=count+1 end;walk()
 local top=86
 if not article then
  local title=U.wrap(clean(item.title),w-48,'b28',3)
  for n,line in ipairs(title) do U.text(line,24,top+(n-1)*32,'b28') end
  top=top+#title*32+18;U.text((item.score or 0)..' points · by '..(item.by or '?')..' · '..age(item.time),24,top,'r20',nil,false,true);top=top+36
  if item.url then U.button('Read the article',24,top,228,56,true);hit('article',nil,24,top,228,56);top=top+90 end
 end
 local capacity=math.max(1,(h-top-44)//29);V.pages=math.max(1,math.ceil(count/capacity));V.page=math.max(1,math.min(V.page,V.pages))
 U.text(V.page..' of '..V.pages,w-24,32,'r20','right',false,true)
 local index=0;local first=(V.page-1)*capacity+1
 visit=function(text,depth,bold)
  index=index+1;if index<first or index>=first+capacity then return end
  local y=top+(index-first)*29
  for indent=1,depth do smudge.rect(24+(indent-1)*20,y-10,3,29,true,true) end
  U.text(text,24+depth*20,y,bold and 'b20' or 'r20')
 end
 walk()
 local status=s.error~='' and s.error or s.loading and 'Loading...' or not article and s.more and 'Swipe up for more comments' or ''
 U.text(U.wrap(clean(status),w-32,'r20',1)[1] or '',w//2,h-20,'r20','center',false,true)
 hit('previous',nil,0,h-44,w//2,44);hit('next',nil,w//2,h-44,w-w//2,44)
end
return V
