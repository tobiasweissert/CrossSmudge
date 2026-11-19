local J=dofile('cp_journal.lua')
local files,writes={},{}
local fail,truncate,corruptRead=false,false,false
local base='/.smudge/saved_data/chess/'
smudge.read_file=function(path,n,offset)
 assert(path:sub(1,#base)==base,'storage escaped app ID');assert(n and n<=321,'unbounded read');assert(offset==0)
 local s=files[path];if not s then return nil end
 if corruptRead and path:match('crossplay') then return 'broken' end
 return s:sub(1,n)
end
smudge.write_file=function(path,s,append)
 assert(path:sub(1,#base)==base,'write escaped app ID');assert(#s<=320 and append==false)
 assert(path~=base..'game.dat','legacy save overwritten')
 writes[#writes+1]=path
 if fail then return false end
 files[path]=truncate and s:sub(1,#s//2) or s;return true
end
local function valid(s) return s=='one' or s=='two' or s=='three' end
local j,s=J.open('chess',256,valid);assert(not s)
assert(j:write('one'));local first=j.slot
assert(j:write('two'));assert(j.slot~=first)
j,s=J.open('chess',256,valid);assert(s=='two')
local goodSlot=j.slot
files[base..'crossplay-'..goodSlot..'.dat']='corrupt'
j,s=J.open('chess',256,valid);assert(s=='one','corrupt latest must recover prior')
local seq,slot=j.sequence,j.slot
fail=true;assert(not j:write('three'));assert(j.sequence==seq and j.slot==slot);fail=false
truncate=true;assert(not j:write('three'));assert(j.sequence==seq and j.slot==slot);truncate=false
j,s=J.open('chess',256,valid);assert(s=='one','torn write lost last valid slot')
corruptRead=true;assert(not j:write('three'));assert(j.sequence==seq and j.slot==slot);corruptRead=false
assert(not j:write('evil'));assert(not j:write(string.rep('x',257)))
files[base..'game.dat']=string.char(1,2,3)..'old-data'
local original=files[base..'game.dat']
local existed,notice=j:preserve('game',76);assert(existed and notice and files[base..'legacy-game.bak']==original)
files[base..'game.dat']='different';j:preserve('game',76)
assert(files[base..'legacy-game.bak']==original and files[base..'game.dat']=='different','backup must not overwrite original or prior backup')
files[base..'game.dat']=string.rep('x',77);j:preserve('game',76)
assert(files[base..'game.dat']==string.rep('x',77),'oversized original altered')
files[base..'legacy-game.bak']=nil;files[base..'game.dat']='old';fail=true
existed,notice=j:preserve('game',76);assert(existed and notice:find('failed') and files[base..'game.dat']=='old')
print('journal corruption, torn-write, readback, legacy-backup tests passed')
