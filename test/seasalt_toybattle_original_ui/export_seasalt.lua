return function(app,name)
 local V,g=app.view,app.state();local M=dofile('core.lua');local cards=dofile('cards.lua');local kinds,colours=cards[1],cards[2];local r=V.renderModel
 local n=math.min(16,#r.tiles-(V.page-1)*16);local values={g.score[1],g.score[2],r.theyHold,r.theirsCount,r.yoursCount,r.bestColourCount,r.bestColour,59-g.at,V.tab,r.handCount,r.yoursCount,r.theirsCount,n,V.page-1,r.pages,r.primaryEnabled and 1 or 0,r.callPoints,r.canCall and 1 or 0}
 for p=1,2 do local c=M.top(g,p);local count=0;for i=1,58 do if g.place[i]==p+4 then count=count+1 end end;values[#values+1]=c==0 and 0 or kinds[c];values[#values+1]=c==0 and 0 or colours[c];values[#values+1]=count end
 local lines={table.concat(values,' '),r.hint,r.primary,r.handTabLabel}
 local supply={};for _,k in ipairs(kinds)do supply[k]=(supply[k]or 0)+1 end
 for i=1,n do local c=r.tiles[(V.page-1)*16+i].c;local seat=(g.place[c]==2 or g.place[c]==4)and 2 or 1;lines[#lines+1]=table.concat({kinds[c],colours[c],V.groupPoints(M,g,kinds[c],seat),supply[kinds[c]],(V.selected==c or V.second==c)and 1 or 0},' ')end
 assert(smudge.write_file(name..'.model',table.concat(lines,'\n')..'\n',false))
end
