assert(TEXT:find('France',1,true));click('Show answer');assert(TEXT:find('Paris',1,true));click('Next card');assert(TEXT:find('Italy',1,true))
click('+');enter('What is 2+2?');enter('4');assert(TEXT:find('What is 2+2?',1,true));restart();on_button('left',true);draw();assert(TEXT:find('What is 2+2?',1,true))
do local U=MODULES['ui.lua'];assert(U.fit('AéB',9)=='A');U.edit('Test','a',4,function(s)assert(s=='ab')end);draw();click('b');click('Done') end
do local M=MODULES['core.lua'];assert(not M.decode('bad'));assert(not M.decode('Deck\na\t\n'));assert(not M.validText('\n',5)) end
-- Failed write retains the previous valid card deck.
FAIL=true;on_button('up',true);draw();click('Rename deck');enter('Unsaved');assert(TEXT:find('Save failed',1,true));FAIL=false;restart();assert(TEXT:find('World capitals',1,true))
on_button('up',true);click('Delete current card');click('Cancel');click('Delete current card');click('Delete card');restart();assert(TEXT:find('of 5',1,true))
on_button('back',true);assert(EXITED)
