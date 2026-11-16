-- Exercise the Sticky replacement branch explicitly.
smudge.get_device=function() return "sticky" end
-- A checksum-valid but unrelated history must not invent repetition or permit undo jumps.
local fen='rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 4 3'
local payload='3 2\n'..fen..'\n'..fen..'\n'..fen
local function sum(s) local a,b=1,0;for i=1,#s do a=(a+s:byte(i))%65521;b=(b+a)%65521 end;return a..':'..b end
local raw='CP1 1 '..#payload..' '..sum('1 '..payload)..'\n'..payload
local assetRead=smudge.read_file
smudge.read_file=function(path,n,offset) if path:match('^art/')then return assetRead(path,n,offset)end;if path:match('crossplay%-1.dat$') then return raw:sub(1,n) end end
smudge.write_file=function() return false end
local app=dofile('main.lua');on_init()
assert(not app.state().over,'unrelated saved history manufactured a draw')
assert(#app.state().history==0,'invalid history accepted')
on_exit()
print('unrelated checksum-valid history rejected')
