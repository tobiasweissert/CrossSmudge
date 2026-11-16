"""Generate only the immutable original pass-direction icon at its source 92px size."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--native',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--asset',type=Path,default=Path(__file__).resolve().parents[2]/'apps/cp-wavelength/art/pass92.raw');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);pgm=a.output/'pass-icon-source.pgm';subprocess.run([str(a.native),'pass','480','800',str(pgm)],check=True);pixels=pgm.read_bytes().split(b'\n',3)[3];out=bytearray()
for y in range(265,357):
 for x in range(36,128,8):
  byte=255
  for bit in range(8):
   if x+bit<128 and pixels[y*480+x+bit]==0:byte&=~(128>>bit)
  out.append(byte)
a.asset.write_bytes(out)
