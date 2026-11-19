local J=dofile('cp_journal.lua');local base='/.smudge/saved_data/wordle/'
local function valid(s)return s=='ONE' or s=='TWO' or s=='THREE' end
local j=J.open('wordle',10,valid)
assert(j:write('ONE'));assert(j:write('TWO'))
local loaded,value=J.open('wordle',10,valid);assert(value=='TWO')
local write=smudge.write_file
smudge.write_file=function(path,body,append) write(path,body:sub(1,#body//2),append);return false end
assert(not loaded:write('THREE'));smudge.write_file=write
local recovered,text=J.open('wordle',10,valid);assert(text=='TWO','torn inactive slot destroyed last good save')
assert(not recovered:write('BAD'));assert(not recovered:write(string.rep('x',1000)))
assert(recovered:write('THREE'))
local final,newtext=J.open('wordle',10,valid);assert(newtext=='THREE')
local raw=smudge.read_file(base..'crossplay-'..final.slot..'.dat',100)
assert(write(base..'crossplay-'..final.slot..'.dat',raw..'x'))
local _,fallback=J.open('wordle',10,valid);assert(fallback=='TWO','corrupt newest slot must fall back')
print('Journal interrupted/trailing/invalid payload checks passed')
