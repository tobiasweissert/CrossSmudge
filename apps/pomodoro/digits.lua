-- Preserve original U.number delegate signature; real licensed sans glyphs.
local N=dofile('numeric_sans.lua')
return function(U,y,text,height,x)
 -- Existing timer caller supplies the old two-digit origin, centered at nx.
 local center=x and x+height*1.25/2 or U.w/2
 local half=math.min(center-14,U.w-14-center)
 return N.draw(tostring(text),math.floor(center-half),math.floor(y),math.floor(half*2),math.floor(height))
end
