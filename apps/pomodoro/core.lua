local M={}
function M.duration(s) return (s.phase==1 and s.focus or s.phase==2 and s.short or s.long)*60 end
function M.next(s,completed)
 if s.phase==1 then
  if completed then s.done=math.min(999,s.done+1) end
  s.phase=s.session==4 and 3 or 2
 else if s.session==4 then s.session=1 else s.session=s.session+1 end;s.phase=1 end
 s.remaining=M.duration(s);s.running=false
end
function M.decode(raw)
 if type(raw)~='string' then return end
 local f,b,l,phase,session,remaining,done,day,label=raw:match('^(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(%d+)\n([^\n]*)$')
 local s={focus=tonumber(f),short=tonumber(b),long=tonumber(l),phase=tonumber(phase),session=tonumber(session),remaining=tonumber(remaining),done=tonumber(done),day=tonumber(day),label=label,running=false}
 if not s.focus or s.focus<1 or s.focus>90 or s.short<1 or s.short>30 or s.long<1 or s.long>60 or s.phase<1 or s.phase>3 or s.session<1 or s.session>4 or s.remaining>M.duration(s) or s.done>999 or #label>48 or label:find('[%z\1-\31]') then return end
 return s
end
function M.encode(s) return table.concat({s.focus,s.short,s.long,s.phase,s.session,s.remaining,s.done,s.day},',')..'\n'..s.label end
-- int32 subtraction intentionally wraps across millis rollover; intervals <2^31 ms.
function M.tick(s,now)
 local delta=now-s.tick;s.tick=now
 if not s.running or delta<=0 then return false end
 s.carry=s.carry+delta;local seconds=s.carry//1000;s.carry=s.carry%1000
 if seconds==0 then return false end
 s.remaining=math.max(0,s.remaining-seconds)
 if s.remaining==0 then M.next(s,true);return true end
 return false
end
return M
