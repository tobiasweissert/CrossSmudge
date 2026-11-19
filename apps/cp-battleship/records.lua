-- Only completions actually observed by this UI are counted. Earlier versions
-- kept no totals; their history cannot be reconstructed from one saved board.
local R={played=0,won=0,streak=0,signature='-',seq=0,slot=0}
local paths={'native-stats-a.dat','native-stats-b.dat'}
local seenRng,seenStatus,seenA,seenB,seenSA,seenSB
local function checksum(s)local n=7;for i=1,#s do n=(n*31+s:byte(i))%65521 end;return n end
local function decode(raw)
 if type(raw)~='string'or #raw>1024 then return end
 local seq,sum,body=raw:match('^UI1:(%d+):(%d+)\n(.*)$');seq,sum=tonumber(seq),tonumber(sum)
 if not seq or math.type(seq)~='integer'or seq<1 or seq>1000000 or checksum(body or'')~=sum then return end
 local played,won,streak,id=body:match('^(%d+) (%d+) (%d+)\n([%d:%-]+)$')
 played,won,streak=tonumber(played),tonumber(won),tonumber(streak)
 if not played or played>65535 or not won or won>played or not streak or streak>won or not id or #id>450 then return end
 if id~='-'then local board=id:match('^%-?%d+:[12]:([0-5]+)$');if not board or #board~=400 or board:sub(201):find('[^01]')then return end end
 return seq,played,won,streak,id
end
function R.load()
 for slot,path in ipairs(paths)do local seq,played,won,streak,id=decode(smudge.read_file(path,1025))
  if seq and seq>R.seq then R.seq,R.slot,R.played,R.won,R.streak,R.signature=seq,slot,played,won,streak,id end
 end
end
function R.observe(g)
 if g.phase~=1 or g.status==0 then return end
 -- Reuse resident string identities; do not rebuild a 400-byte signature on
 -- every repaint of the same completed board. No extra board copy is kept.
 if g.rng==seenRng and g.status==seenStatus and g.a==seenA and g.b==seenB and g.sa==seenSA and g.sb==seenSB then return end
 seenRng,seenStatus,seenA,seenB,seenSA,seenSB=g.rng,g.status,g.a,g.b,g.sa,g.sb
 local id=g.rng..':'..g.status..':'..g.a..g.b..g.sa..g.sb
 if id==R.signature then return end
 R.played=math.min(65535,R.played+1)
 if g.status==1 then R.won=math.min(65535,R.won+1);R.streak=math.min(65535,R.streak+1)else R.streak=0 end
 R.signature=id;R.dirty=true
end
function R.begin()
 seenRng,seenStatus,seenA,seenB,seenSA,seenSB=nil,nil,nil,nil,nil,nil
 if R.signature~='-'then R.signature='-';R.dirty=true end
end
function R.save()
 if not R.dirty then return true end
 if R.seq>=1000000 then smudge.log('Battleship record journal sequence limit reached');return false end
 local body=R.played..' '..R.won..' '..R.streak..'\n'..R.signature
 local seq=R.seq+1;local slot=R.slot==1 and 2 or 1;local raw='UI1:'..seq..':'..checksum(body)..'\n'..body
 if not smudge.write_file(paths[slot],raw)or smudge.read_file(paths[slot],1025)~=raw then smudge.log('Battleship record save failed');return false end
 R.seq,R.slot,R.dirty=seq,slot,false;return true
end
return R
