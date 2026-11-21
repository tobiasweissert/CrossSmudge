#pragma once
// Firmware-owned executable closure. The SD package supplies art and journal data only.
// test/notes_dictation/run.py checks these bytes against the versioned package.
#include <cstring>
namespace dictation {
inline const char* module(const char* name) {
  if (!strcmp(name, "sticky_enhanced.lua"))
    return R"NotesCode(local N=dofile('core.lua');local S=dofile('storage.lua');local F=dofile('graphics.lua')
local doc,lines,index,selected,screen,choice='',{},1,1,'library',1
-- Only a card loaded through load() owns the document and journal sequence.
local loaded=false
local hits={};local summaries={};local pages={};local viewPage=1;local pageAnchor=0;local closed=false;local original;local offset=0;local notice='';local w,h=480,800;local margin=25;local pageRows=8;local pending=''
local function save()
 if not loaded then return true end
 collectgarbage('collect')
 if not S.save(doc) then notice='Save failed. Keep app open and retry.';return false end
 return true
end
local function load(index_)
 index=index_;selected=1;offset=0;pageAnchor=0;viewPage=1;doc='';lines={};collectgarbage('collect');S.scope(index)
 local restored=S.load(N.validate)
 if restored then doc=restored else
  local raw=smudge.read_file('note'..index..'.md',3073)
  doc=raw and N.validate(raw) or ''
  if raw and not N.validate(raw) then notice='Import rejected: over 3072 bytes/97 lines or NUL.' end
 end
 lines=N.parse(doc,true);loaded=true;screen='note'
end
local function redraw()smudge.request_update()end
local options={'Resume','Clear checked lines','Convert to checklist','Convert to page','Import from note file','Help','Save and close'}
local dictationCard
if smudge.request_dictation then options[#options+1]='Dictate (OpenAI)' end
local function change(new)
 local parsed=N.parse(new,true);summaries[index]=nil;pageAnchor=0;viewPage=1;S.last=nil;offset=0;doc=new;lines=parsed;selected=math.min(selected,#lines);redraw()
end
-- Validate Unicode scalar values without enabling network libraries in Notes.
local function validUtf8(s)
 local i=1
 while i<=#s do
  local b=s:byte(i);i=i+1
  if b>=128 then
   if b<194 or b>244 then return false end
   local n=b<224 and 1 or b<240 and 2 or 3
   local c=s:byte(i)
   if not c or (b==224 and c<160) or (b==237 and c>=160) or (b==240 and c<144) or (b==244 and c>=144) then return false end
   for _=1,n do c=s:byte(i);if not c or c<128 or c>191 then return false end;i=i+1 end
  end
 end
 return true
end
-- The trusted native editor returns only on Done; never replace existing text.
function on_dictation_reviewed(text)
 local card=dictationCard;dictationCard=nil
 if not loaded or closed or index~=card or screen~='dictating' then return end
 screen='note'
 if type(text)=='string' and #text>0 then
  local nextDoc=doc..(doc=='' and '' or '\n')..text
  if validUtf8(text) and not text:find('[%z\1-\8\11-\31\127]') and N.validate(nextDoc) then change(nextDoc);notice=''
  else notice='Transcript invalid or card full. Nothing added.' end
 end
 redraw()
end
local function operate()
 if not loaded then screen='library';redraw();return end
 if choice==1 then screen='note'
 elseif choice==2 or choice==5 then pending=choice==2 and 'clear' or 'import';screen='confirm';choice=1
 elseif choice==3 then change(N.tolist(doc,lines));screen='note'
 elseif choice==4 then change(N.strip(doc,lines));screen='note'
 elseif choice==6 then screen='help'
 elseif choice==8 and smudge.request_dictation then if save() then dictationCard=index;screen='dictating';smudge.request_dictation() else screen='note' end
 elseif save() then loaded=false;screen='library';choice=index end
 redraw()
end
function on_init()
 closed=false;F.init();local ok,o=pcall(smudge.get_orientation);if ok then original=o end;w,h=smudge.get_bounds();redraw()
end
local function hit(b,kind,value)F.hit(hits,b,kind,value);return b end
local function button(label,b,kind,value,outline)
 F.rect(b,not outline,false,1);F.center(label,F.box(b.x,b.y+1,b.w,b.h),F.fit(label,b.w-20,20),not outline);hit(b,kind,value)
end
local function summary(i)
 if summaries[i] then return summaries[i] end
 S.scope(i);local text=S.load(N.validate);if not text then local raw=smudge.read_file('note'..i..'.md',3073);text=N.validate(raw) and raw or '' end
 local parsed=N.parse(text,true);local item={total=0,done=0,preview=''}
 for _,v in ipairs(parsed) do local row=N.row(v);if row.mark then item.total=item.total+1;if row.checked then item.done=item.done+1 end elseif item.preview=='' then item.preview=text:sub(row.text,row.last) end end
 summaries[i]=item;S.scope(index);return item
end
local function progress(b,done,total)
 F.rect(b,false,false,1);if done>0 and total>0 then F.rect(F.box(b.x,b.y,math.max(3,b.w*done//total),b.h),true) end
end
local function wrap(text,width,size)
 local result,current={},''
 for word in text:gmatch('%S+') do
  if current~='' and F.width(current..' '..word,size)>width then result[#result+1]=current;current='' end
  if F.width(word,size)>width then
   for char in word:gmatch('[%z\1-\127\194-\244][\128-\191]*') do
    if current~='' and F.width(current..char,size)>width then result[#result+1]=current;current='' end
    current=current..char
   end
  else current=current=='' and word or current..' '..word end
 end
 if current~='' then result[#result+1]=current end
 if #result==0 then result[1]='' end;return result
end
local function plan(top,bottom,reserved)
 pages={{}};local y=top;local total,done=0,0;local maxLines=math.max(1,(bottom-top-32)//42)
 for i,v in ipairs(lines) do
  local row=N.row(v);if row.mark then total=total+1;if row.checked then done=done+1 end end
  local text=N.slice(doc:sub(row.text,row.last),i==selected and offset+1 or 1,3072)
  local broken=wrap(text,w-32-(row.mark and 52 or 0),20)
  for start=1,#broken,maxLines do
   local chunk={};for n=start,math.min(#broken,start+maxLines-1) do chunk[#chunk+1]=broken[n] end
   local height=math.max(row.mark and 72 or 42,#chunk*42+(row.mark and 32 or 12))
   if y+height>bottom and #pages[#pages]>0 then pages[#pages+1]={};y=top end
   local block={index=i,lines=chunk,row=row,y=y,h=height};pages[#pages][#pages[#pages]+1]=block;y=y+height
  end
 end
 if #pages>1 and not reserved then return plan(top,bottom-33,true)end
 if #pages==1 and #pages[1]>0 then
  local list=pages[1];local last=list[#list];local tasks=0;for _,block in ipairs(list) do if block.row.mark then tasks=tasks+1 end end
  local grow=tasks>0 and math.min(36,math.max(0,(bottom-last.y-last.h)//tasks)) or 0;local shift=0
  for _,block in ipairs(list) do block.y=block.y+shift;if block.row.mark then block.h=block.h+grow;shift=shift+grow end end
 end
 if pageAnchor~=selected then
  -- Find the first continuation page of a selected long paragraph.
  for page,list in ipairs(pages) do local found=false;for _,block in ipairs(list) do if block.index==selected then found=true;break end end;if found then viewPage=page;break end end
  pageAnchor=selected
 end
 viewPage=math.max(1,math.min(viewPage,#pages));return total,done
end
function on_draw()
 w,h=smudge.get_bounds();hits={};smudge.clear();local title=screen=='library' and 'NOTES' or 'NOTE '..index
 if screen=='library' then F.header(title,w) else F.header('',w);F.text(title,16,26,20,true) end
 if screen~='library' then
  smudge.rect(w-79,5,66,66,true,false);smudge.bitmap(w-62,22,32,32,F.settings,false)
  hit(F.box(w-79,5,66,66),'menu')
 end
 local footer=F.box(16,h-68,w-32,52)
 if screen=='library' then
  local capacity=math.max(1,(h-68-12-119+12)//116);local start=(choice-1)//capacity*capacity+1
  for i=start,math.min(8,start+capacity-1) do
   local item=summary(i);local y=119+(i-start)*116;local badge=F.box(16,y+14,76,76)
   if item.total>0 then F.rect(badge,true);F.center(item.done..'/'..item.total,badge,F.fit(item.done..'/'..item.total,68,10),true)
   else F.rect(badge,false,false,3);for n=0,2 do smudge.rect(34,y+34+n*16,n==2 and 24 or 40,3,true,true) end end
   F.text('Note '..i,104,y+(item.total>0 and 27 or item.preview~='' and 23 or 40),20)
   if item.total>0 then progress(F.box(104,y+72,w-120,14),item.done,item.total)
   elseif item.preview~='' then
    local text=item.preview;while #text>0 and F.width(text..'...',10)>w-120 do text=N.slice(text,1,math.max(0,#text-1)) end
    F.text(text..(text~=item.preview and '...' or ''),104,y+72,10)
   end
   if i==choice then F.corners(F.box(12,y,w-24,104),10,2) end
   hit(F.box(16,y,w-32,104),'library',i)
  end
  F.center(((start-1)//capacity+1)..' / '..((7)//capacity+1),F.box(16,h-101,w-32,21),10)
  button('HOW TO IMPORT',footer,'help')
 elseif screen=='note' then
  local total,done=0,0;for _,v in ipairs(lines) do local row=N.row(v);if row.mark then total=total+1;if row.checked then done=done+1 end end end
  local top=119
  if total>0 then
   local label=done==total and 'ALL DONE' or (total-done)..' LEFT OF '..total;local width=F.width(label,10)+24
   F.text(label,16,112,10);progress(F.box(16+width,111,w-32-width,14),done,total);smudge.rect(16,139,w-32,1,true,true);top=140
  end
  plan(top,h-92)
  for _,block in ipairs(pages[viewPage]) do
   local row=block.row;local x=row.mark and 68 or 16;local textY=block.y+(block.h-#block.lines*42)//2
   if row.mark then
    F.round(F.box(16,textY+1,40,40),4,false,false,3)
    if row.checked then F.round(F.box(25,textY+10,22,22),2,true) end
   end
   for n,line in ipairs(block.lines) do
    F.text(line,x,textY+9+(n-1)*42,20)
    if row.checked and line~='' then smudge.rect(x,textY+(n-1)*42+21,math.min(w-16-x,F.width(line,20)),2,true,true) end
   end
   if selected==block.index then smudge.rect(w-8,block.y,3,math.min(block.h,h-118-block.y),true,true) end
   hit(F.box(16,block.y,w-32,block.h),'line',block.index)
  end
  if #pages>1 then F.center(viewPage..' / '..#pages,F.box(16,h-113,w-32,21),10)end
  if done>0 then
   button('OPTIONS',F.box(16,footer.y,(w-44)//2,52),'menu')
   button('CLEAR DONE',F.box(28+(w-44)//2,footer.y,(w-44)//2,52),'clear',nil,true)
  else button('OPTIONS',footer,'menu') end
 elseif screen=='menu' then
  local pitch=math.min(76,(h-119-100)//#options)
  for i,label in ipairs(options) do local b=F.box(16,119+(i-1)*pitch,w-32,pitch)
   F.text(label:upper(),16,b.y+(b.h-25)//2,F.fit(label,w-32,20));if i<#options then smudge.rect(16,b.y+b.h-1,w-32,1,true,true) end
   if choice==i then F.corners(b,10,2) end;hit(b,'option',i)
  end
  button('BACK',footer,'resume')
 elseif screen=='confirm' then
  F.sheet(pending=='clear' and 'Remove all checked lines? The original import file stays unchanged.' or 'Replace this card with its note file? Your saved edits will be replaced.',F.box(16,119,w-32,h-211))
  button('KEEP IT',F.box(16,footer.y,(w-44)//2,52),'confirm',1)
  button(pending=='clear' and 'CLEAR DONE' or 'IMPORT',F.box(28+(w-44)//2,footer.y,(w-44)//2,52),'confirm',2,true)
 elseif screen=='help' then
  local prose='Copy note1.md through note8.md into this app folder. Each card accepts 3072 bytes and 97 lines. Use - [ ] task or - [x] task for checkboxes. Touch a row to toggle it. Up and Down choose a line; Left and Right scroll long text. Swipe to turn pages. OPTIONS imports a file, converts a card, or clears checked lines with confirmation. Imports stay unchanged; journals hold your edits. Phone editing and sleep notes require host support.'
  local text=wrap(prose,w-32,10);for i,line in ipairs(text) do if 119+(i-1)*26<h-110 then F.text(line,16,119+(i-1)*26,10) end end
  button('BACK',footer,'back')
 end
 if notice~='' then F.center(notice,F.box(16,h-110,w-32,25),10) end
end
function on_button(btn,pressed)
 if not pressed or closed then return end
 if screen=='note' and (btn=='page_back' or btn=='page_forward') then viewPage=math.max(1,math.min(#pages,viewPage+(btn=='page_forward' and 1 or -1)));redraw();return end
 if btn=='page_back' then btn='up' elseif btn=='page_forward' then btn='down' end
 if btn=='back' then
  if screen=='library' then closed=true;smudge.exit()
  elseif screen=='note' then screen='menu';choice=1
  elseif screen=='menu' then if save() then loaded=false;screen='library';choice=index end
  elseif screen=='help' and not loaded then screen='library'
  else screen='menu';choice=1 end
 elseif screen=='library' then
  if btn=='up' then choice=(choice-2)%8+1 elseif btn=='down' then choice=choice%8+1
  elseif btn=='confirm' then notice='';load(choice) end
 elseif screen=='note' then
  if btn=='up' then selected=(selected-2)%#lines+1;offset=0 elseif btn=='down' then selected=selected%#lines+1;offset=0
  elseif btn=='confirm' then
   local line=N.row(lines[selected])
   if line.mark then summaries[index]=nil;S.last=nil;collectgarbage('collect');doc=N.toggle(doc,selected,lines);line.checked=not line.checked;lines[selected]=N.pack(line) end
  elseif btn=='right' or btn=='left' then
   local l=N.row(lines[selected]);offset=math.max(0,math.min(math.max(0,l.last-l.text), offset+(btn=='right' and 20 or -20)))
  end
 elseif screen=='menu' then
  if btn=='up' then choice=(choice-2)%#options+1 elseif btn=='down' then choice=choice%#options+1
  elseif btn=='confirm' then operate() end
 elseif screen=='confirm' then
  if btn=='up' or btn=='down' then choice=3-choice
  elseif btn=='confirm' then
   if choice==2 then
    if pending=='clear' then change(N.clear(doc,lines)) else
     local raw=smudge.read_file('note'..index..'.md',3073)
     if N.validate(raw) then change(raw);notice='' else notice='Import failed: missing, invalid, or too large.' end
    end
   end
   screen='note'
  end
 end
 redraw()
end
function on_touch(x,y,event)
 if event~='tap' or closed or x<0 or y<0 or x>=w or y>=h then return end
 local item=F.at(hits,x,y);if not item then return end
 local action=item.action;local value=item.value
 if action=='library' then choice=value;on_button('confirm',true)
 elseif action=='line' then selected=value;offset=0;on_button('confirm',true)
 elseif action=='option' then choice=value;operate()
 elseif action=='confirm' then choice=value;on_button('confirm',true)
 elseif action=='menu' then on_button('back',true)
 elseif action=='clear' then choice=2;operate()
 elseif action=='resume' then screen=loaded and 'note' or 'library';redraw()
 elseif action=='help' then screen='help';redraw()
 elseif action=='back' then on_button('back',true) end
end
function on_swipe(dir)
 if closed then return end
 if screen=='note' then if dir=='up' or dir=='left' then on_button('page_forward',true) elseif dir=='down' or dir=='right' then on_button('page_back',true) end
 elseif screen=='library' then
  local capacity=math.max(1,(h-68-12-119+12)//116)
  choice=math.max(1,math.min(8,choice+((dir=='up' or dir=='left') and capacity or -capacity)));redraw()
 else on_button(dir,true) end
end
function on_update()end
function on_exit()if loaded then save() end;closed=true;if original then pcall(smudge.set_orientation,original) end end
return {state=function()return doc,lines,screen,index end,hits=function()return hits end,pages=function()return pages,viewPage end}
)NotesCode";
  if (!strcmp(name, "core.lua"))
    return R"NotesCode(-- NotesCore.cpp rules translated from CrossPlay. Offsets are Lua 1-based.
local N={}
-- Seven array slots per row avoid seven hash entries per row on constrained Lua.
-- One shared metatable keeps the readable field interface, without per-row closures.
local slots={first=1,last=2,finish=3,text=4,mark=5,checked=6,offset=7}
local row_mt={__index=function(t,k)return rawget(t,slots[k])end,
 __newindex=function(t,k,v)rawset(t,slots[k],v)end}
function N.validate(s)
 if type(s)~='string' or #s>3072 or s:find('%z') then return nil end
 local n=0;for _ in s:gmatch('\n') do n=n+1 end
 if n>96 then return nil end
 return s
end
function N.pack(line)
 return string.pack('<I2I2I2I2I2B',line.first,line.last,line.finish,line.text,line.mark or 0,line.checked and 1 or 0)
end
function N.row(raw)
 if type(raw)=='table' then return raw end
 local first,last,finish,text,mark,checked=string.unpack('<I2I2I2I2I2B',raw)
 if mark==0 then mark=nil end
 return setmetatable({first,last,finish,text,mark,checked==1,0},row_mt)
end
function N.parse(s,compact)
 local lines={};local start=1
 while start<=#s+1 do
  local nl=s:find('\n',start,true);local stop=(nl or (#s+1))-1
  local last=stop;if s:sub(last,last)=='\r' then last=last-1 end
  local raw=s:sub(start,last);local indent,mark,rest=raw:match('^([ \t]*)[-*+] %[(.)%](.*)$')
  local line=setmetatable({start,last,nl or #s,start,nil,false,0},row_mt)
  if mark and (mark==' ' or mark=='x' or mark=='X') and (rest=='' or rest:sub(1,1)==' ') then
   line.mark=start+#indent+3;line.checked=mark~=' ';line.text=line.mark+2+(rest~='' and 1 or 0)
  end
  lines[#lines+1]=compact and N.pack(line) or line
  if not nl then break end;start=nl+1
 end
 return lines
end
function N.toggle(s,index,parsed)
 local raw=(parsed or N.parse(s,true))[index];if not raw then return s end
 local line=N.row(raw);if not line.mark then return s end
 return s:sub(1,line.mark-1)..(line.checked and ' ' or 'x')..s:sub(line.mark+1)
end
-- Bounded cold-path conversions build one <=3072-byte string rather than
-- retaining one newly allocated string for every distinct line simultaneously.
function N.clear(s,parsed)
 local out='';for _,raw in ipairs(parsed or N.parse(s,true)) do
  local line=N.row(raw)
  if not line.checked then out=out..s:sub(line.first,line.finish) end
 end;return out
end
function N.strip(s,parsed)
 local out='';for _,raw in ipairs(parsed or N.parse(s,true)) do
  local l=N.row(raw)
  out=out..s:sub(l.mark and l.text or l.first,l.finish)
 end;return out
end
function N.tolist(s,parsed)
 local out='';for _,raw in ipairs(parsed or N.parse(s,true)) do
  local l=N.row(raw);local prefix=(l.last>=l.first and not l.mark) and '- [ ] ' or ''
  if #out+#prefix+l.finish-l.first+1>3072 then return s end
  out=out..prefix..s:sub(l.first,l.finish)
 end
 return N.validate(out) and out or s
end
-- Avoid cutting multibyte UTF-8 at either display boundary; bytes remain untouched.
function N.slice(s,first,last)
 while first>1 and (s:byte(first) or 0)>=128 and (s:byte(first) or 0)<192 do first=first-1 end
 local next=last+1
 while next>first and (s:byte(next) or 0)>=128 and (s:byte(next) or 0)<192 do next=next-1 end
 return s:sub(first,next-1)
end
return N
)NotesCode";
  if (!strcmp(name, "storage.lua"))
    return R"NotesCode(-- Fixed app-relative journal slots. No user-provided paths; never touches other apps.
local S={seq=0,slot=0,last=nil}
local paths={'state-a.dat','state-b.dat'}
local function checksum(s)
 local h=7;for i=1,#s do h=(h*31+s:byte(i))%65521 end;return h
end
local function parse(raw,validate)
 if type(raw)~='string' or #raw>4096 then return nil end
 local n,h,p=raw:match('^CP1:(%d+):(%d+)\n(.*)$')
 n=tonumber(n);h=tonumber(h)
 if not n or math.type(n)~='integer' or n<1 or n>1000000 or not p or checksum(p)~=h then return nil end
 local value=validate(p);if not value then return nil end
 return value,n,p
end
function S.scope(index)
 assert(math.type(index)=='integer' and index>=1 and index<=8)
 paths={'note-'..index..'-a.dat','note-'..index..'-b.dat'}
 S.seq=0;S.slot=0;S.last=nil
end
function S.load(validate)
 local best
 for i=1,2 do
  local value,n,p=parse(smudge.read_file(paths[i],4097),validate)
  if value and n>S.seq then best=value;S.seq=n;S.slot=i;S.last=p end
  collectgarbage('collect')
 end
 return best
end
function S.save(payload)
 if payload==S.last then return true end
 if #payload>4000 or S.seq>=1000000 then smudge.log('CrossPlay save limit reached');return false end
 local n=S.seq+1;local slot=S.slot==1 and 2 or 1
 local raw='CP1:'..n..':'..checksum(payload)..'\n'..payload
 if not smudge.write_file(paths[slot],raw) then smudge.log('CrossPlay save write failed');return false end
 if smudge.read_file(paths[slot],4097)~=raw then smudge.log('CrossPlay save readback failed');return false end
 S.seq=n;S.slot=slot;S.last=payload;return true
end
return S
)NotesCode";
  if (!strcmp(name, "graphics.lua"))
    return R"NotesCode(-- Actual immutable CrossPlay glyph masks; one compact string per cut, loaded once.
-- Ink widths and differential fixed-point advances match EpdFont/GfxRenderer.
local F={cap={[10]=13,[14]=18,[20]=25,[30]=38}}
local fonts={}
function F.init()F.settings=assert(smudge.read_file('art/original-settings32.raw',128));for _,size in ipairs({10,14,20,30})do fonts[size]=assert(smudge.read_file('art/original'..size..'.bin',50000),'Missing original Jersey cut')end end
local function glyph(font,code)
 if code<32 or code>255 then code=63 end
 local w,h,a,b,left,top,lo,hi=font:byte((code-32)*8+1,(code-32)*8+8)
 return w,h,a+b*256,left>127 and left-256 or left,top>127 and top-256 or top,lo+hi*256
end
function F.clean(text)return tostring(text or '') end
-- Offline apps do not request network-only utf8 library; decode valid input locally.
local function codes(text)
 local i=1;return function()
  if i>#text then return end;local c=text:byte(i);i=i+1
  if c>=192 then local n=c<224 and 1 or c<240 and 2 or 3;c=c%(2^(6-n))
   for _=1,n do local b=text:byte(i)or 128;i=i+1;c=c*64+b%64 end
  end;return c
 end
end
-- Preserve imported UTF-8 bytes in storage; unsupported original cut glyphs
-- remain readable on screen as codepoints rather than disappearing.
function F.clean(text)
 local out={}
 for c in codes(tostring(text or ''))do
  local _,_,advance=glyph(fonts[20],c)
  if c>=32 and c<=255 and advance>0 then
   if c<128 then out[#out+1]=string.char(c)else out[#out+1]=string.char(192+c//64,128+c%64)end
  else out[#out+1]=string.format('[U+%04X]',c)end
 end;return table.concat(out)
end
function F.width(text,size)
 local x,minX,maxX,advance=0,0,0,nil
 for c in codes(F.clean(text))do
  if advance then x=x+(advance+8)//16 end
  local w,_,a,left=glyph(fonts[size],c);minX=math.min(minX,x+left);maxX=math.max(maxX,x+left+w);advance=a
 end;return maxX-minX
end
function F.text(text,x,y,size,paper)
 local font=fonts[size];local advance
 for c in codes(F.clean(text))do
  if advance then x=x+(advance+8)//16 end
  local w,h,a,left,top,offset=glyph(font,c)
  if w>0 and h>0 then smudge.bitmap(x+left,y+F.cap[size]-top,w,h,font:sub(offset+1,offset+((w+7)//8)*h),paper or false)end
  advance=a
 end
end
function F.center(text,b,size,paper)
 text=F.clean(text);F.text(text,b.x+(b.w-F.width(text,size))//2,b.y+(b.h-F.cap[size])//2,size,paper)
end
function F.fit(text,width,largest)
 text=F.clean(text)
 for _,size in ipairs({30,20,14,10}) do if size<=(largest or 30) and F.width(text,size)<=width then return size end end
 return 14
end
function F.wrap(text,width,size)
 local lines,current={},''
 for word in F.clean(text):gmatch('%S+') do
  if current~='' and F.width(current..' '..word,size)>width then lines[#lines+1]=current;current='' end
  if F.width(word,size)>width then
   for i=1,#word do local char=word:sub(i,i)
    if F.width(current..char,size)>width and current~='' then lines[#lines+1]=current;current='' end
    current=current..char
   end
  else current=current=='' and word or current..' '..word end
 end
 if current~='' then lines[#lines+1]=current end
 return lines
end
function F.block(text,b,largest,maxlines,paper)
 local chosen,lines=14,{}
 for _,size in ipairs({30,20,14,10}) do if size<=(largest or 30) then
  local wrapped=F.wrap(text,b.w,size)
  if #wrapped<=(maxlines or 3) and #wrapped*(F.cap[size]+8)-8<=b.h then chosen,lines=size,wrapped;break end
 end end
 if #lines==0 then lines=F.wrap(text,b.w,14) end
 local step=F.cap[chosen]+8;local y=b.y+(b.h-(#lines*step-8))//2
 for _,line in ipairs(lines) do F.center(line,{x=b.x,y=y,w=b.w,h=F.cap[chosen]},chosen,paper);y=y+step end
end
function F.box(x,y,w,h)return {x=math.floor(x),y=math.floor(y),w=math.floor(w),h=math.floor(h)} end
function F.rect(b,fill,paper,stroke)
 if fill then smudge.rect(b.x,b.y,b.w,b.h,true,not paper) else smudge.rect(b.x,b.y,b.w,b.h,false,stroke or 1,not paper) end
end
function F.round(b,r,fill,paper,stroke)smudge.rounded_rect(b.x,b.y,b.w,b.h,r,fill,not paper,stroke or 1)end
function F.corners(b,arm,weight,paper)
 for _,dx in ipairs({0,b.w-arm}) do for _,dy in ipairs({0,b.h-weight}) do smudge.rect(b.x+dx,b.y+dy,arm,weight,true,not paper) end end
 for _,dx in ipairs({0,b.w-weight}) do for _,dy in ipairs({0,b.h-arm}) do smudge.rect(b.x+dx,b.y+dy,weight,arm,true,not paper) end end
end
function F.header(title,w,right)
 smudge.rect(0,0,w,76,true,true);smudge.rect(0,80,w,3,true,true)
 local size=F.fit(title,w-32-(right and 120 or 0),30);F.text(title,16,(76-F.cap[size])//2,size,true)
 if right then F.text(right,w-16-F.width(right,20),26,20,true) end
end
function F.button(label,b,selected,size)
 F.round(b,10,selected,false,selected and 1 or 1)
 F.center(label,b,size or F.fit(label,b.w-24,20),selected)
end
function F.hit(list,b,action,value)
 list[#list+1]={box=b,action=action,value=value};return b
end
function F.at(list,x,y)
 for _,hit in ipairs(list) do local b=hit.box;if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then return hit end end
end
function F.pips(list,w,y,pages,current,action)
 local left=(w-pages*34)//2
 for page=1,pages do local x=left+(page-1)*34
  smudge.circle(x+17,y+22,5,true,page==current)
  if page~=current then smudge.circle(x+17,y+22,5,false,true) end
  F.hit(list,F.box(x,y,34,44),action,page)
 end
end
function F.sheet(text,b)
 local lines=F.wrap(text,b.w,20);local top=b.y+math.max(0,(b.h-#lines*42)//2)
 for i,line in ipairs(lines)do F.text(line,b.x,top+(i-1)*42+9,20)end
end
return F
)NotesCode";
  return nullptr;
}
}  // namespace dictation
