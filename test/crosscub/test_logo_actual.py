#!/usr/bin/env python3
"""Selected bitmap through actual production rotation/drawPixel and logo helper."""
from pathlib import Path
import argparse,re,subprocess,tempfile,hashlib
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--runner',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
packed=bytes(int(v,16)for v in re.findall(r'0x([0-9a-f]{2})',(R/'src/images/CrosscubBear120.h').read_text().split('{',1)[1]))
expected=bytes(255 if b&(128>>i)else 0 for b in packed for i in range(8))
with tempfile.TemporaryDirectory(prefix='logo-',dir=a.output)as tmp:
 d=Path(tmp);(d/'draw.lua').write_text("assert(smudge.get_device()=='sticky');local w,h=smudge.get_bounds();smudge.clear();smudge.crosscub_logo((w-120)//2,(h-120)//2);capture('logo.pgm')")
 for w,h in [(480,800),(800,480)]:
  r=subprocess.run([str(a.runner.resolve()),'draw.lua','2097152',str(w),str(h)],cwd=d,capture_output=True,text=True);assert r.returncode==0,r.stderr
  raw=(d/'logo.pgm').read_bytes();pix=raw.split(b'\n',3)[3];cropped=bytes(pix[y*w+x]for y in range((h-120)//2,(h+120)//2)for x in range((w-120)//2,(w+120)//2));assert cropped==expected,(w,h)
  (a.output/f'production-logo-{w}x{h}.pgm').write_bytes(raw)
  print('PASS actual production oriented logo',w,h,hashlib.sha256(cropped).hexdigest())
