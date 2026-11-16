T.open();T.update();T.complete('[11]');T.update();T.complete(T.story(11,'Synthetic article',0));T.tap('story',1);T.update()
T.failWrites(true);T.complete(string.rep('Body line.\n',20));T.failWrites(false)
assert(T.state().error=='Article loaded; cache write failed')
local font=assert(smudge.read_file('art/reading_serif_11.bin',32768));local i=(65-32)*8+1
local w,h,_,_,_,_,lo,hi=font:byte(i,i+7);local offset=lo+hi*256;local glyph=font:sub(offset+1,offset+((w+7)//8)*h)
local bitmap=smudge.bitmap;local detected=false
smudge.bitmap=function(x,y,gw,gh,data,...)
 if gw==w and gh==h and data==glyph then detected=true;assert(y+h<=119,'error caption overlaps the original reader body')end
 return bitmap(x,y,gw,gh,data,...)
end
T.shot('article-cache-failure');assert(detected,'actual failure caption did not draw')
on_exit();print('PASS failed-write status clears original body text')
