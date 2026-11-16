local M=dofile('core.lua');local read=smudge.read_file;local saves={};local reject=false
smudge.read_file=function(path,n,o)if saves[path]then return saves[path]:sub((o or 0)+1,(o or 0)+n)end;if path:match('%.dat$')then return nil end;return read(path,n,o)end;smudge.write_file=function(path,v)if reject and path:match('^native%-')then return false end;saves[path]=v;return true end
local kind=M.total and'yahtzee'or M.cols==8 and'minesweeper'or M.neighbours and'go'or'hex'
local app
local function draw()on_draw();capture('progress.pgm')end
if kind=='go'then
 app=dofile('main.lua');on_init();draw();on_button('down',true);on_button('down',true);on_button('confirm',true);draw();on_button('confirm',true);on_button('back',true);draw();on_button('confirm',true);draw();on_button('confirm',true);on_button('confirm',true);draw();on_button('confirm',true);draw();assert(app.state().phase==2);on_button('back',true);draw();on_exit()
elseif kind=='hex'then
 app=dofile('main.lua');on_init();draw();on_button('down',true);on_button('down',true);on_button('confirm',true);draw();on_button('confirm',true);on_button('back',true);draw();on_button('confirm',true);draw()
 for r=0,10 do local l=app.layout();on_touch(l.x+2*l.a,l.y+l.half+2*l.half*r,'tap');if r<10 then on_touch(l.x+32*l.a,l.y+l.half+2*l.half*r+10*l.half,'tap')end end;draw();assert(app.state().status==1);on_button('back',true);draw();on_exit()
elseif kind=='minesweeper'then
 app=dofile('main.lua');on_init();draw();on_button('confirm',true);draw();local l=app.layout();on_touch(l.x+l.cell//2,l.y+l.cell//2,'tap')
 for p=1,80 do if app.state().b[p]&1==0 and app.state().b[p]&2==0 then on_touch(l.x+(p-1)%8*l.cell+l.cell//2,l.y+(p-1)//8*l.cell+l.cell//2,'tap')end end;draw();assert(app.state().status==2);on_button('back',true);draw();on_exit()
else
 local g=M.new(123456);for p=1,2 do for c=1,14 do g.cards[p][c]=0 end end;g.cards[2][1]=-1;g.turn=2;g.rolls=3;g.d={2,2,2,2,2};assert(dofile('storage.lua').save('1|'..M.encode(g)))
 app=dofile('main.lua');on_init();draw();on_button('confirm',true);draw();on_update();draw();assert(M.over(app.state()));on_button('back',true);draw();on_exit()
end
local encoded=M.encode(app.state())
local function sum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
for path,raw in pairs(saves)do if path:match('^native%-')then
 local tag,seq,body=raw:match('^(%w+):(%d+):%d+\n(.*)$');assert(tag);local head,rest=body:match('^([^\n]+)\n(.*)$');local values={};for v in head:gmatch('%d+')do values[#values+1]=tonumber(v)end
 if kind=='go'then values[10],values[11]=65534,1 elseif kind=='hex'then values[4],values[5]=65534,1 elseif kind=='minesweeper'then values[1],values[2]=65535,65534 else values[1],values[2],values[3],values[4],values[5]=65535,65534,1575,65535,6 end
 body=table.concat(values,' ')..'\n'..rest;saves[path]=tag..':'..seq..':'..sum(body)..'\n'..body
end end
app=dofile('main.lua');on_init();draw();local R=app.preferences or(type(app.records)=='function'and app.records()or app.records);assert((R.played or R.wins+R.losses)==65535);assert(M.encode(app.state())==encoded);capture('long-record.pgm')
local g=app.state();local map
if kind=='go'then local b,w=M.score(g);map=table.concat({g.n,g.turn,g.phase,g.moves,g.passes,g.last,g.captured[1],g.captured[2],g.komi,g.handicap,R.seat,g.solo,0,b,w},' ')..'\n'..g.b..'\n'..g.dead..'\nmenu '..R.wins..' '..R.losses..' '..R.lastWon..' '..R.margin..' 0'
elseif kind=='hex'then local b={};for i=1,121 do b[i]=g.b[i]end;map=table.concat(b)..'\n'..table.concat({g.turn,g.status,R.lastMove,2,R.seat},' ')..'\nmenu '..R.wins..' '..R.losses..' 0'
elseif kind=='minesweeper'then local b={};for i,v in ipairs(g.b)do b[i]=v end;map=table.concat(b)..'\n'..g.status..' menu '..R.played..' '..R.won..' 0'
else local v={g.turn,g.rolls,g.held};for i=1,5 do v[#v+1]=g.d[i]end;for p=1,2 do for c=1,14 do v[#v+1]=g.cards[p][c]end end;map=table.concat(v,' ')..'\nmenu '..table.concat({R.played,R.won,R.best,R.yahtzees,R.face},' ')end
assert(fixtureWrite('long-record.map',map..'\n',false));on_exit()
-- Corrupt optional presentation journals never invalidate the actual core game.
for path in pairs(saves)do if path:match('^native%-')then saves[path]='corrupt\n'..string.rep('9',4097)end end
app=dofile('main.lua');on_init();draw();assert(M.encode(app.state())==encoded,'corrupt optional record invalidated game');on_exit()
