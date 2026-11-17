"""Deterministically pack the owner-selected Welcome bear for Crosscub.
Usage: python convert_selected_bear.py SOURCE_PNG OUTPUT_DIR [--prepared-120 PNG]
Source is original generated raster artwork, not an invented SVG/vector source.
"""
import argparse,hashlib,json
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--prepared-120',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with Image.open(a.source)as f:master=f.convert('L');master_size=f.size
master.save(a.output/'Crosscub-Welcome-Bear.png','PNG')
if a.prepared_120:
 with Image.open(a.prepared_120)as f:assert f.size==(120,120);small=f.convert('L')
 assert set(small.tobytes())<={0,255},'Prepared preview must be truly black/white'
else:
 small=Image.new('L',(120,120),255);resized=master.copy();resized.thumbnail((112,112),Image.Resampling.LANCZOS);small.paste(resized,((120-resized.width)//2,(120-resized.height)//2));small=small.point(lambda x:255 if x>=160 else 0)
mono=small.convert('1',dither=Image.Dither.NONE);packed=mono.tobytes();assert len(packed)==1800
header='#pragma once\n#include <cstdint>\n\n// Crosscub Welcome bear: owner-selected option 1.\n// 120x120 pixels, row-major MSB-first; 1=white, 0=black.\nstatic const uint8_t CrosscubBear120[] = {\n'
for i in range(0,1800,15):header+='  '+', '.join(f'0x{x:02x}'for x in packed[i:i+15])+',\n'
header+='};\nstatic_assert(sizeof(CrosscubBear120) == 1800, "Crosscub bear bitmap size");\n'
(a.output/'CrosscubBear120.h').write_text(header)
mono.save(a.output/'Crosscub-Welcome-Bear-120.png','PNG')
decoded=Image.frombytes('1',(120,120),packed).convert('L');assert decoded.tobytes()==small.tobytes()
decoded.resize((480,480),Image.Resampling.NEAREST).save(a.output/'Crosscub-Welcome-Bear-firmware-preview.png','PNG')
ink=decoded.point(lambda x:255 if x==0 else 0);bbox=ink.getbbox();assert bbox and 0<bbox[0]<bbox[2]<120 and 0<bbox[1]<bbox[3]<120
manifest={'selected_option':1,'selection':'Owner: I like all 3, but lets go with 1','name':'Welcome','description':'Standing and waving cub, book under arm','source_format':'original generated raster PNG','editable_vector_source_available':False,'master_dimensions':master_size,'firmware_dimensions':[120,120],'packed_bytes':len(packed),'ink_bbox':bbox,'black_pixels':decoded.tobytes().count(0),'pixel_polarity':'MSB first, 1 white,0 black','matches_owner_seen_120px_preview':bool(a.prepared_120),'files':{f.name:{'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}for f in sorted(a.output.iterdir())if f.is_file()and f.name!='selected-artwork-manifest.json'}}
(a.output/'selected-artwork-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
