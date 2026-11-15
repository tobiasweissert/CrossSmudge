-- CrossPlay WordleCore port. MIT; see PROVENANCE.md.
local C={}
local function word(s) return type(s)=='string' and #s==5 and s:match('^[A-Za-z]+$') and s:upper() or nil end
function C.score(guess,answer)
 local marks,spare={},{}
 for i=1,5 do local a,g=answer:sub(i,i),guess:sub(i,i)
  if a==g then marks[i]=4 else marks[i]=2;spare[a]=(spare[a] or 0)+1 end
 end
 for i=1,5 do local g=guess:sub(i,i);if marks[i]~=4 and (spare[g] or 0)>0 then marks[i]=3;spare[g]=spare[g]-1 end end
 return marks
end
function C.mergeKey(key,mark) return math.max(key,mark) end
local function days(y,m,d)
 y=y-(m<=2 and 1 or 0);local era=y//400;local yoe=y-era*400
 local doy=(153*(m+(m>2 and -3 or 9))+2)//5+d-1
 return era*146097+yoe*365+yoe//4-yoe//100+doy-719468
end
local launch=days(2021,6,19)
function C.dayIndex(y,m,d) return days(y,m,d)-launch end
function C.dateOfDay(index)
 local z=index+launch+719468;local era=z//146097;local doe=z-era*146097
 local yoe=(doe-doe//1460+doe//36524-doe//146096)//365
 local doy=doe-(365*yoe+yoe//4-yoe//100);local mp=(5*doy+2)//153
 local d=doy-(153*mp+2)//5+1;local m=mp+(mp<10 and 3 or -9)
 return yoe+era*400+(m<=2 and 1 or 0),m,d
end
local function iso(s)
 local y,m,d=s:match('^(%d%d%d%d)%-(%d%d)%-(%d%d)$');y,m,d=tonumber(y),tonumber(m),tonumber(d)
 if not y or m<1 or m>12 or d<1 or d>31 then return nil end;return C.dayIndex(y,m,d)
end
function C.parseAnswerLine(s)
 s=s:gsub('[\r\n ]+$','');if #s~=16 or s:sub(11,11)~=' ' then return nil end
 local day=iso(s:sub(1,10));local w=word(s:sub(12));if day and w then return day,w end
end
function C.parseDailyJson(s)
 if #s>8192 then return nil end
 local date=s:match('"print_date"%s*:%s*"([^"]*)"');local w=word(s:match('"solution"%s*:%s*"([^"]*)"'))
 if date and w then return iso(date),w end
end
function C.answerFor(s,day) if day<0 then return nil end;return word(s:sub(day*5+1,day*5+5)) end
function C.putAnswer(s,day,w)
 if day<0 or day>36524 or not word(w) then return s end;local at=day*5
 if #s<at+5 then s=s..string.rep('.',at+5-#s) end;return s:sub(1,at)..w:upper()..s:sub(at+6)
end
function C.lastKnownDay(s) for d=#s//5-1,0,-1 do if C.answerFor(s,d) then return d end end;return -1 end
function C.isWord(list,guess)
 local lo,hi=0,#list//5
 while lo<hi do local mid=lo+(hi-lo)//2;local w=list:sub(mid*5+1,mid*5+5)
  if w==guess then return true elseif w<guess then lo=mid+1 else hi=mid end
 end
 return false
end
function C.start(day,answer)
 if not word(answer) then return nil end
 return {day=day,answer=answer:upper(),rows={},marks={},keys={},typing='',status='Playing'}
end
function C.type(g,letter)
 if g.status~='Playing' or #g.typing>=5 or type(letter)~='string' or not letter:match('^[A-Z]$') then return false end
 g.typing=g.typing..letter;return true
end
function C.erase(g) if g.status~='Playing' or #g.typing==0 then return false end;g.typing=g.typing:sub(1,-2);return true end
local function apply(g,guess)
 local marks=C.score(guess,g.answer);g.rows[#g.rows+1]=guess;g.marks[#g.marks+1]=marks
 for i=1,5 do local c=guess:sub(i,i);g.keys[c]=math.max(g.keys[c] or 0,marks[i]) end
 if guess==g.answer then g.status='Won' elseif #g.rows==6 then g.status='Lost' end
end
function C.submit(g,isWord)
 if g.status~='Playing' then return 'Over' end;if #g.typing<5 then return 'Short' end
 if not isWord(g.typing) then return 'NotAWord' end
 apply(g,g.typing);g.typing='';return g.status=='Playing' and 'Scored' or g.status
end
function C.save(g) return g.answer..(#g.rows>0 and ' '..table.concat(g.rows,' ') or '') end
function C.load(day,s)
 if type(s)~='string' or #s<5 or #s>41 or (#s-5)%6~=0 then return nil end
 local answer=word(s:sub(1,5));if not answer then return nil end;local g=C.start(day,answer)
 for at=6,#s,6 do if g.status~='Playing' or s:sub(at,at)~=' ' then return nil end
  local w=word(s:sub(at+1,at+5));if not w then return nil end;apply(g,w)
 end
 return g
end
function C.resultOf(s,day) return day>=0 and (s:byte(day+1) or 0) or 0 end
function C.putResult(s,day,value)
 if day<0 or day>36524 or value<0 or value>255 then return s end
 if #s<=day then s=s..string.rep('\0',day+1-#s) end;return s:sub(1,day)..string.char(value)..s:sub(day+2)
end
function C.statsFor(results,today)
 local s={played=0,won=0,streak=0,wins={0,0,0,0,0,0}}
 for i=1,#results do local r=results:byte(i);local v=r&127
  if r&128~=0 then if v>=1 and v<=6 then s.played=s.played+1;s.won=s.won+1;s.wins[v]=s.wins[v]+1 elseif v==7 then s.played=s.played+1 end end
 end
 local function won(d) local r=C.resultOf(results,d);return r&128~=0 and r&127>=1 and r&127<=6 end
 local d=today;if not won(d) then if C.resultOf(results,d)&127==7 then return s end;d=d-1 end
 while d>=0 and won(d) do s.streak=s.streak+1;d=d-1 end
 return s
end
return C
