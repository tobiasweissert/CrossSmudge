-- Scalable seven-segment numerals use existing primitives, no extra font/cache.
return function(U,y,text,height,x)
 local digits={['0']=0x3f,['1']=0x06,['2']=0x5b,['3']=0x4f,['4']=0x66,['5']=0x6d,['6']=0x7d,['7']=0x07,['8']=0x7f,['9']=0x6f,['-']=0x40}
 local dw=height*0.55;local gap=height*0.12;local thick=math.max(3,math.floor(height*0.09))
 x=x or (U.w-#text*(dw+gap))/2
 for i=1,#text do
  local c=text:sub(i,i);local dx=x+(i-1)*(dw+gap)
  if c=='o' then smudge.circle(math.floor(dx+dw/3),math.floor(y+height*0.12),math.floor(height*0.08),false,math.max(2,thick//2))
  else
   local bits=digits[c] or 0
   local segments={{thick,0,dw-2*thick,thick},{dw-thick,thick,thick,height/2-1.5*thick},{dw-thick,height/2+thick/2,thick,height/2-1.5*thick},{thick,height-thick,dw-2*thick,thick},{0,height/2+thick/2,thick,height/2-1.5*thick},{0,thick,thick,height/2-1.5*thick},{thick,height/2-thick/2,dw-2*thick,thick}}
   for j,s in ipairs(segments)do if (bits&(1<<(j-1)))~=0 then smudge.rounded_rect(math.floor(dx+s[1]),math.floor(y+s[2]),math.floor(s[3]),math.floor(s[4]),2,true)end end
  end
 end
end
