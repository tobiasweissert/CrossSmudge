-- Real production int32/float32 VM, original fonts/raster; HTTP/storage only are
-- fixtures. Preserve accepted 64KiB article and128KiB thread limits.
for _,kind in ipairs({'newlines','word','normal','utf8'})do
 T.reset();T.update();T.complete('[11]');T.update();T.complete(T.story(11,'Long article',1));T.tap('story',1);T.update()
 local body=kind=='newlines'and string.rep('a\n',32764)..'\nTAIL'or kind=='word'and string.rep('X',65531)..'\nTAIL'or kind=='normal'and string.rep('Read this article. ',3448)..'\nTAIL'or string.rep('Café 😀 ',5460)..'\nTAIL'
 assert(#body<=65536);T.complete(body);assert(T.state().articleText==body and T.view().pages>1)
 local pages=T.view().pages
 on_swipe('up');on_draw();assert(T.view().page==2 and T.view().pages==pages)
 while T.view().page<pages do on_swipe('up')end;on_draw()
 assert(table.concat(T.view().visibleLines):find('TAIL',1,true),'article tail is not drawable')
 T.shot('article-'..kind..'-last-page');on_exit();body=nil;collectgarbage('collect')
end
local ids={};for id=31,70 do ids[#ids+1]=id end
T.reset();T.update();T.complete('[11]');T.update();T.complete(T.story(11,'Bounded comments',40,'['..table.concat(ids,',')..']'));T.tap('comments',1)
for id=31,63 do
 T.update();assert(T.requests()[#T.requests()].url:find('/'..id..'.json',1,true),'comment cursor skipped '..id)
 -- More input while work is active must not consume another deferred id.
 if id>36 then on_swipe('up');on_swipe('up')end
 T.complete('{"id":'..id..',"type":"comment","by":"writer","text":"'..string.rep('a\\n',2000)..'"}')
 if (id-30)%6==0 then
  on_draw();while T.view().page<T.view().pages do on_swipe('up')end
  on_swipe('up');on_swipe('up')
 end
end
assert(#T.state().comments==32 and not T.state().more and T.state().error=='Thread session limit reached')
for i,c in ipairs(T.state().comments)do assert(c.id==30+i,'stored comments are not contiguous')end
on_draw();T.shot('thread-session-limit');on_exit();collectgarbage('collect')
print('PASS real original-font 64KiB newline/word/normal/UTF8 article tails and128KiB comment cursor/memory limits')
