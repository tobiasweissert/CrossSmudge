local files, writes = {}, {}
local base = '/.smudge/saved_data/sudoku/'
local fault = nil
smudge.read_file = function(path, cap, offset)
  assert(path:sub(1,#base) == base, 'storage escaped app save directory')
  assert(type(cap)=='number' and cap <= 4161 and cap >= 0, 'unbounded save read')
  local s=files[path]
  if not s then return nil end
  return s:sub((offset or 0)+1,(offset or 0)+cap)
end
smudge.write_file = function(path, s, append)
  assert(path:sub(1,#base)==base and not append)
  assert(path~=base..'state.dat', 'legacy overwritten')
  writes[#writes+1]=path
  if fault=='refuse' then return false end
  files[path]=fault=='truncate' and s:sub(1,#s//2) or s
  return true
end
local J=dofile('cp_journal.lua')
local function valid(s) return s:match('^valid:%d+$')~=nil end
local function open() return J.open('sudoku',4096,valid) end
local j,s=open();assert(not s)
assert(j:write('valid:1'))
assert(j:write('valid:2'))
local raw1,raw2=files[base..'crossplay-1.dat'],files[base..'crossplay-2.dat']
j,s=open();assert(s=='valid:2')
-- Every truncation of the newest slot recovers the older complete transaction.
for n=0,#raw1-1 do
  files[base..'crossplay-1.dat']=raw1:sub(1,n)
  j,s=open();assert(s=='valid:1','truncation lost fallback at '..n)
end
files[base..'crossplay-1.dat']=raw1
-- Every one-byte mutation fails checksum or format, never becoming save code.
for n=1,#raw1 do
  files[base..'crossplay-1.dat']=raw1:sub(1,n-1)..string.char((raw1:byte(n)+1)%256)..raw1:sub(n+1)
  j,s=open();assert(s=='valid:1','mutation accepted at '..n)
end
files[base..'crossplay-1.dat']=raw1
j,s=open();local seq=j.sequence
fault='truncate';assert(not j:write('valid:3'));assert(j.sequence==seq)
j,s=open();assert(s=='valid:2','partial write overwrote only valid save')
fault='refuse';assert(not j:write('valid:3'));j,s=open();assert(s=='valid:2')
fault=nil;assert(j:write('valid:3'));j,s=open();assert(s=='valid:3')
assert(not j:write('return os.execute("bad")'))
files[base..'state.dat']='legacy bytes\0including NUL'
local found,notice=j:preserve('state',4096)
assert(found and notice:find('backed up'))
assert(files[base..'legacy-state.bak']==files[base..'state.dat'])
local backup=files[base..'legacy-state.bak']
files[base..'state.dat']='changed old save'
j:preserve('state',4096)
assert(files[base..'legacy-state.bak']==backup)
files[base..'state.dat']=string.rep('x',4097)
found,notice=j:preserve('state',4096)
assert(found and notice:find('too large'))
assert(#files[base..'state.dat']==4097)
assert(files[base..'legacy-state.bak']==backup)
print('journal truncation, corruption, failed writes, backup tests passed')
