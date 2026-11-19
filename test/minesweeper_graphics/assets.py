"""Lossless repack of CrossPlay 1-bit Epd glyphs into shipped Lua bitmap format."""
import pathlib,re,shutil,hashlib,json
R=pathlib.Path(__file__).resolve().parents[2]; C=pathlib.Path('/root/github/crossplay'); A=R/'apps/cp-minesweeper'; out=A/'art';out.mkdir(exist_ok=True)
provenance={}
for size in (20,30):
 p=C/f'src/apps_local/ui/fonts/toybox_{size}.h';s=p.read_text();provenance[str(p.relative_to(C))]=hashlib.sha256(p.read_bytes()).hexdigest()
 bits=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s.split('Bitmaps')[1].split('};')[0]))
 rows=[list(map(int,m.split(','))) for m in re.findall(r'\{([\d, -]+)\}',s.split('Glyphs[]')[1].split('};')[0])]
 rows=rows[:59]  # UI uses ASCII space through Z only.
 header=bytearray();payload=bytearray()
 for code,(w,h,advance,left,top,n,offset) in enumerate(rows,32):
  allowed='0123456789MINESWPROF -' if size==30 else 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 /?.>-'
  if chr(code) not in allowed:w=h=0
  start=len(rows)*7+len(payload);header.extend([w,h,(advance+8)//16,left%256,top,start%256,start//256])
  raw=bytearray([255]*(((w+7)//8)*h))
  for y in range(h):
   for x in range(w):
    bit=y*w+x
    if bits[offset+bit//8]&(128>>(bit%8)):raw[y*((w+7)//8)+x//8]&=~(128>>(x%8))
  payload.extend(raw)
 (out/f'font{size}.bin').write_bytes(header+payload)
p=C/'src/apps_local/ui/ToyboxIcons.h';s=p.read_text();bits=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s.split('icon_mineflag_32_bits[]')[1].split('};')[0]));assert len(bits)==128
for side in (20,36,40):
 raw=bytearray([255]*(((side+7)//8)*side))
 for y in range(side):
  for x in range(side):
   sx=x*32//side;sy=y*32//side
   if not bits[sy*4+sx//8]&(128>>(sx%8)):raw[y*((side+7)//8)+x//8]&=~(128>>(x%8))
 (out/f'flag{side}.raw').write_bytes(raw)
shutil.copyfile(C/'site/assets/fonts/OFL-Jersey25.txt',A/'OFL-Jersey25.txt')
shutil.copyfile(C/'freeink-sdk/libs/assets/Icons/lucide/LICENSE',A/'LICENSE-Lucide.txt')
provenance['src/apps_local/ui/ToyboxIcons.h']=hashlib.sha256(p.read_bytes()).hexdigest()
(out/'sources.json').write_text(json.dumps(provenance,indent=2)+'\n')
