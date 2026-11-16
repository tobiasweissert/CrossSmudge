-- Exact UI facts observed by this port, without inventing earlier lifetime records.
local R={played=0,won=0,best=0,yahtzees=0,face=0,signature='-'};local paths={'native-record-a.dat','native-record-b.dat'};local seq,slot,dirty=0,0,false;local seen
local function sum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
local function decode(Y,raw)
 if type(raw)~='string'or #raw>1200 then return end
 local n,check,body=raw:match('^YZ1:(%d+):(%d+)\n(.*)$');n,check=tonumber(n),tonumber(check)
 if not n or math.type(n)~='integer'or n<1 or n>1000000 or sum(body or'')~=check then return end
 local p,w,b,y,f,signature=body:match('^(%d+) (%d+) (%d+) (%d+) (%d+)\n(.*)$');p,w,b,y,f=tonumber(p),tonumber(w),tonumber(b),tonumber(y),tonumber(f)
 if not p or not w or not b or not y or not f then return end
 for _,v in ipairs({p,w,b,y,f})do if math.type(v)~='integer'or v>65535 then return end end
 if w>p or f>6 then return end
 if signature~='-'then local g=Y.decode(signature);if not g or not Y.over(g)then return end end
 return n,p,w,b,y,f,signature
end
function R.load(Y)
 for at,path in ipairs(paths)do local n,p,w,b,y,f,s=decode(Y,smudge.read_file(path,1201));if n and n>seq then seq,slot,R.played,R.won,R.best,R.yahtzees,R.face,R.signature=n,at,p,w,b,y,f,s end end
end
function R.observe(Y,g)
 if not Y.over(g)or seen==g then return end;seen=g
 local key=Y.encode(g);if key==R.signature then return end
 R.played=math.min(65535,R.played+1);local yours,theirs=Y.total(g.cards[1]),Y.total(g.cards[2]);if yours>theirs then R.won=math.min(65535,R.won+1)end;R.best=math.max(R.best,yours)
 R.yahtzees=math.min(65535,R.yahtzees+(g.cards[1][12]==50 and 1 or 0)+g.cards[1][14]);R.signature=key;dirty=true
end
function R.note(face)if face>0 then R.face=face;dirty=true end end
function R.begin()seen=nil;if R.signature~='-'then R.signature='-';dirty=true end end
function R.save()
 if not dirty then return true end
 if seq>=1000000 then smudge.log('Yahtzee UI record limit reached');return false end
 local body=table.concat({R.played,R.won,R.best,R.yahtzees,R.face},' ')..'\n'..R.signature;local raw='YZ1:'..(seq+1)..':'..sum(body)..'\n'..body;local at=slot==1 and 2 or 1
 if not smudge.write_file(paths[at],raw)or smudge.read_file(paths[at],1201)~=raw then smudge.log('Yahtzee UI record save failed');return false end
 seq,slot,dirty=seq+1,at,false;return true
end
return R
