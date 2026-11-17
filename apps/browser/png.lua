-- Validate the narrow SRP1 palette PNG envelope before the native codec owns it.
-- PNGdec remains the decoder; this is never a raw bitmap blit.
local P={}
local crcTable={}
for i=0,255 do local c=i;for _=1,8 do c=(c>>1)~((c&1)~=0 and 0xedb88320 or 0)end;crcTable[i]=c end
local function crc(read,offset,n)
 local c=0xffffffff
 for at=offset,offset+n-1,2048 do
  local count=math.min(2048,offset+n-at);local s=read(at,count);if type(s)~='string'or #s~=count then return end
  for i=1,#s do c=(c>>8)~crcTable[(c~s:byte(i))&255]end
 end
 return ~c
end
function P.validate(read,size,width,height)
 if math.type(size)~='integer'or size<67 or size>65536 then return false end
 local h=read(0,33);if type(h)~='string'or #h~=33 or h:sub(1,8)~='\137PNG\r\n\26\n'or h:sub(13,16)~='IHDR'then return false end
 local function u32(s,p)return s:byte(p)*16777216+s:byte(p+1)*65536+s:byte(p+2)*256+s:byte(p+3)end
 local w,hh=u32(h,17),u32(h,21)
 if u32(h,9)~=13 or w<1 or hh<1 or w>width or hh>height or h:byte(25)~=1 or h:byte(26)~=3 or h:sub(27,29)~='\0\0\0'then return false end
 local at,plte,idat,ended,count=8,false,false,false,0
 while at<size do
  local s=read(at,8);if type(s)~='string'or #s~=8 then return false end
  local n=u32(s,1);local kind=s:sub(5,8);count=count+1
  if n<0 or n>size-at-12 or count>256 or ended then return false end
  local checksum=read(at+8+n,4);if type(checksum)~='string'or #checksum~=4 or crc(read,at+4,n+4)~=u32(checksum,1)then return false end
  if at==8 then if kind~='IHDR'or n~=13 then return false end
  elseif kind=='PLTE'then
   if plte or idat or n~=6 then return false end
   local palette=read(at+8,6);if palette~='\0\0\0\255\255\255'and palette~='\255\255\255\0\0\0'then return false end;plte=true
  elseif kind=='IDAT'then if not plte then return false end;idat=true
  elseif kind=='IEND'then if n~=0 or not idat then return false end;ended=true
  else return false end -- no APNG, profiles, text, transparency, or other metadata
  at=at+n+12
 end
 return ended and at==size
end
return P
