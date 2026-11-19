return function(app,name)
 local g,V=app.state(),app.view;local M=app.core;local phase=g.phase;local reveal=V.currentView=='reveal';local a,b='CHARMING','ANNOYING';local c,d='COLD','HOT';local id=phase==1 and g.a or g.pair
 if id>0 then a,b=M.word(id):match('^(.-) / (.*)$')end;if g.b>0 then c,d=M.word(g.b):match('^(.-) / (.*)$')end
 local r=V.record;local values={reveal and g.round or g.round+1,g.total,math.max(0,g.round-1),g.guess,g.target,g.points,(reveal and g.round==1 or not reveal and g.round==0)and 1 or 0,g.mode==2 and 1 or 0,(g.target>g.guess and g.call==1 or g.target<g.guess and g.call==2)and 1 or 0,V.peek and 1 or 0,V.everRevealed and 1 or 0,V.nudge and 1 or 0,(V.sessionStarted or phase>0 or g.round>0)and 1 or 0,g.b==0 and 1 or 0,(V.currentView=='pass'and not V.justAbandoned)and 0 or V.abandoned,r.rounds,r.points}
 for _,n in ipairs(r.buckets)do values[#values+1]=n end
 assert(smudge.write_file(name..'.wave',table.concat(values,' ')..'\n'..a..'\n'..b..'\n'..c..'\n'..d..'\n'));assert(smudge.write_file(name..'.txt',V.currentView))
end
