local C=dofile('cp_core.lua')
assert(type(C.valid)=='function','save position validator required')
local good={
 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
 'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2',
 '7k/6Q1/6K1/8/8/8/8/8 b - - 0 1',
 '8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1'}
for _,fen in ipairs(good) do assert(C.valid(assert(C.parseFen(fen))),'reject valid '..fen) end
local bad={
 '8/8/8/8/8/8/8/8 w - - 0 1',
 '7k/8/8/8/8/8/8/KK6 w - - 0 1',
 '7k/8/8/8/8/8/k7/K7 w - - 0 1',
 '7k/8/8/8/8/8/8/K6P w - - 0 1',
 '7k/8/8/8/8/8/8/4K3 w K - 0 1',
 '7k/8/8/8/8/8/8/K7 w - e4 0 1',
 '7k/8/8/8/8/8/8/K7 w - e6 0 1',
 '7k/8/4N3/4p3/8/8/8/K7 w - e6 0 1',
 '7k/8/8/8/8/8/8/K6R w - - 0 1'}
for _,fen in ipairs(bad) do local p=C.parseFen(fen);assert(not p or not C.valid(p),'accepted invalid '..fen) end
for _,fen in ipairs({'','xxx','8/8/8/8/8/8/8/8 w - - 0 999999999999999999999999999999999999','8/8/8/8/8/8/8/8 z - - 0 1'}) do
 local p=C.parseFen(fen);assert(not p or not C.valid(p),'accepted malformed')
end
local p=C.start();p.squares[5]=7;assert(not C.valid(p),'unknown piece')
p=C.start();p.squares[5]=nil;assert(not C.valid(p),'missing board slot')
p=C.start();p.whiteToMove=0;assert(not C.valid(p),'nonboolean turn')
p=C.start();p.castling=16;assert(not C.valid(p),'invalid rights')
p=C.start();p.halfmoveClock=-1;assert(not C.valid(p),'negative clock')
print('save position validation passed')
