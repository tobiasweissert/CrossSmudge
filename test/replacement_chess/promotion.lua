-- Same required four-promotion behaviour against the baseline or replacement.
local count=0
if smudge.read_file('baseline-marker',10) then
 dofile('legacy-engine.lua')
 local s={board={},turn=1,castling=0,ep_sq=0,halfmove=0,fullmove=1}
 for i=1,64 do s.board[i]=0 end
 s.board[8]=-6;s.board[9]=1;s.board[57]=6;s.board[56]=-1
 for _,m in ipairs(gen_moves(s)) do if (m&127)==9 and ((m>>7)&127)==1 then count=count+1 end end
else
 local C=dofile('cp_core.lua')
 local p=assert(C.parseFen('7k/P7/8/8/8/8/7p/K7 w - - 0 1'))
 for _,m in ipairs(C.legal(p)) do if m.from==48 and m.to==56 then count=count+1 end end
end
assert(count==4,'required four legal promotions; got '..count)
print('four promotion choices passed')
