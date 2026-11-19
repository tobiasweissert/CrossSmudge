return function(app,name)
 local g,v=app.state(),app.view();local d=v.draft;local n={g.seed,g.map,g.turn,g.phase,g.winner,g.ending,g.taken[1],g.taken[2],g.medals[1],g.medals[2],g.frozen[1],g.frozen[2],g.tick,g.regions,g.special,#g.slots}
 for seat=1,2 do for k=1,8 do n[#n+1]=g.rack[seat][k]end end;for seat=1,2 do for k=1,8 do n[#n+1]=g.discard[seat][k]end end
 for i,p in ipairs(g.slots)do n[#n+1]=p;n[#n+1]=g.tiles[i]end
 n[#n+1]=#d.steps;n[#n+1]=d.step;n[#n+1]=d.effect and 1 or 0;n[#n+1]=d.base and 1 or 0
 for _,step in ipairs(d.steps)do for _,field in ipairs(step)do n[#n+1]=field end end
 assert(smudge.write_file(name..'.model',table.concat(n,' ')..'\n',false))
end
