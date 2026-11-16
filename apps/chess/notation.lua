-- ChessCore.cpp moveToSan, presentation only: never mutates the visible position.
local N={}
local function file(s)return string.char(97+(s&7))end
local function rank(s)return string.char(49+(s>>3))end
function N.san(C,p,m,legal)
 local piece=p.squares[m.from]&7;local text=''
 if m.flags&8~=0 then text=m.to&7==2 and'O-O-O'or'O-O'
 elseif piece==1 then text=(m.flags&1~=0 and file(m.from)..'x'or'')..file(m.to)..rank(m.to);if m.flags&16~=0 then text=text..'='..('PNBRQK'):sub(m.promotion,m.promotion)end
 else
  text=('PNBRQK'):sub(piece,piece);local ambiguous,sameFile,sameRank=false,false,false
  for _,other in ipairs(legal or C.legal(p))do if other.to==m.to and other.from~=m.from and p.squares[other.from]&7==piece then ambiguous=true;sameFile=sameFile or other.from&7==m.from&7;sameRank=sameRank or other.from>>3==m.from>>3 end end
  if ambiguous then text=text..(not sameFile and file(m.from)or not sameRank and rank(m.from)or file(m.from)..rank(m.from))end
  text=text..(m.flags&1~=0 and'x'or'')..file(m.to)..rank(m.to)
 end
 local u=C.make(p,m);if C.check(p)then text=text..(#C.legal(p)==0 and'#'or'+')end;C.unmake(p,m,u);return text
end
function N.between(C,fen,target)
 local p=C.parseFen(fen);local legal=C.legal(p)
 for _,m in ipairs(legal)do local u=C.make(p,m);local matches=C.fen(p)==target;C.unmake(p,m,u);if matches then return N.san(C,p,m,legal)end end
 return'?'
end
return N
