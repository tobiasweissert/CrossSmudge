T.files['note1.md']='- [ ] Shopping\n- [ ] Bread\n- [x] Milk\n- [ ] A readable paragraph.'
local a=T.open();T.shot('notes');T.action('library',1);T.shot('notes-note');T.button('down');T.button('confirm');T.shot('notes-check');local prior=select(1,a.state());T.action('line',2);assert(select(1,a.state())~=prior);T.action('line',2);assert(select(1,a.state())==prior);T.button('back');T.shot('notes-menu');T.action('option',2);T.shot('notes-confirm');T.action('confirm',1);T.button('back');T.shot('notes-restored');on_exit()

-- Every write still belongs to the note actually opened, including failed close.
a=T.open();T.button('confirm');T.button('confirm');T.button('back')
for _=1,6 do T.button('down')end
T.fail(true);T.button('confirm');assert(select(3,a.state())=='menu' and not T.exited());T.shot('notes-save-failure');T.fail(false);T.button('confirm');assert(select(3,a.state())=='library');on_exit()
for i=2,8 do assert(not T.files['note-'..i..'-a.dat']and not T.files['note-'..i..'-b.dat'],'unvisited note written')end
-- Long, valid imports remain paged and reachable; storage retains original UTF-8.
T.files['note2.md']='Café 漢字\n'..string.rep('A long paragraph with readable words. ',70)..'LAST SENTENCE'
a=T.open();T.button('down');T.button('confirm');local raw=select(1,a.state());assert(raw==T.files['note2.md']);on_draw();local pages=a.pages();assert(#pages>1)
local all={};for _,page in ipairs(pages)do for _,block in ipairs(page)do for _,line in ipairs(block.lines)do all[#all+1]=line end end end;assert(table.concat(all,' '):find('LAST SENTENCE',1,true),'long final text unreachable')
for _=2,#pages do T.button('page_forward')end;T.shot('notes-long-last');T.button('back');for _=1,5 do T.button('down')end;T.button('confirm');T.shot('notes-help');on_exit()
