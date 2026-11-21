local fs={['note1.md']='Original'}
local redraws,requests=0,0
smudge={request_update=function() redraws=redraws+1 end,request_dictation=function() requests=requests+1 end,
 read_file=function(p) return fs[p] end,write_file=function(p,v)fs[p]=v;return true end,
 get_bounds=function()return 480,800 end}
assert(not utf8 and not io and not package)
local failedWrite=false
local write=smudge.write_file;smudge.write_file=function(p,v) if failedWrite then return false end;return write(p,v) end
smudge.log=function()end
local read=smudge.read_file;smudge.read_file=function(p) if p:sub(1,4)=='art/' then return 'neutral asset' end;return read(p) end
local request=smudge.request_dictation;smudge.request_dictation=nil
local app=installNotes(request);assert(not pcall(dofile,'unused.lua'));
on_init();on_button('confirm',true)
local function begin()
 on_button('back',true);on_button('up',true);on_button('confirm',true)
end
begin();assert(requests==1,'no native dictation request');on_dictation_reviewed('');assert(app.state()=='Original')
begin();on_dictation_reviewed(string.rep('a',3072));assert(app.state()=='Original')
begin();on_dictation_reviewed('bad'..string.char(255));assert(app.state()=='Original')
begin();on_dictation_reviewed('Neutral phrase');assert(app.state()=='Original\nNeutral phrase')
on_dictation_reviewed('duplicate');assert(app.state()=='Original\nNeutral phrase');
local count=requests;failedWrite=true;begin();assert(requests==count and app.state()=='Original\nNeutral phrase');failedWrite=false
begin();assert(requests==count+1);rejectOOM();assert(app.state()=='Original\nNeutral phrase');on_exit()
app=installNotes(request);on_init();on_button('confirm',true);assert(app.state()=='Original\nNeutral phrase')
assert(fs['note1.md']=='Original');assert(fs['note-1-a.dat'] or fs['note-1-b.dat'])
print('PASS real Notes menu, cancel, combined byte limit, UTF-8, reviewed append, duplicate, journal and original')
