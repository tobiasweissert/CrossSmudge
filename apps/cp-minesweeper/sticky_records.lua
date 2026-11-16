-- Only known completed fields are counted; no earlier lifetime history is inferred.
local R={played=0,won=0,last='',signature='-'};local paths={'native-record-a.dat','native-record-b.dat'};local seq,slot,dirty=0,0,false;local seen,status
local function sum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
local function decode(M,raw)
 if type(raw)~='string'or #raw>1200 then return end
 local n,check,body=raw:match('^MN1:(%d+):(%d+)\n(.*)$');n,check=tonumber(n),tonumber(check)
 if not n or math.type(n)~='integer'or n<1 or n>1000000 or sum(body or'')~=check then return end
 local played,won,signature,last=body:match('^(%d+) (%d+)\n([^\n]+)\n(.*)$');played,won=tonumber(played),tonumber(won)
 if not played or not won or math.type(played)~='integer'or math.type(won)~='integer'or played>65535 or won>played then return end
 if signature~='-'then local g=M.decode(signature);if not g or g.status<2 then return end end
 local g=M.decode(last);if not g or g.status<2 then return end
 return n,played,won,signature,last
end
function R.load(M)
 for at,path in ipairs(paths)do local n,p,w,s,last=decode(M,smudge.read_file(path,1201));if n and n>seq then seq,slot,R.played,R.won,R.signature,R.last=n,at,p,w,s,last end end
end
function R.observe(M,g)
 if g.status<2 or seen==g and status==g.status then return end;seen,status=g,g.status
 local encoded=M.encode(g);if encoded==R.signature then return end
 R.played=math.min(65535,R.played+1);if g.status==2 then R.won=math.min(65535,R.won+1)end
 R.last,R.signature,dirty=encoded,encoded,true
end
function R.begin()seen,status=nil,nil;if R.signature~='-'then R.signature='-';dirty=true end end
function R.save()
 if not dirty or R.last==''then return true end
 if seq>=1000000 then smudge.log('Minesweeper UI record limit reached');return false end
 local body=R.played..' '..R.won..'\n'..R.signature..'\n'..R.last;local raw='MN1:'..(seq+1)..':'..sum(body)..'\n'..body;local at=slot==1 and 2 or 1
 if not smudge.write_file(paths[at],raw)or smudge.read_file(paths[at],1201)~=raw then smudge.log('Minesweeper record save failed');return false end
 seq,slot,dirty=seq+1,at,false;return true
end
local history,key
function R.history(M)if key~=R.last then key=R.last;history=key~=''and M.decode(key)or nil end;return history end
return R
