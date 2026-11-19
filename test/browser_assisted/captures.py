#!/usr/bin/env python3
"""Replay original synthetic input fixtures through the production font adapter."""
import pathlib,os,subprocess,json,hashlib,struct,zlib
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R)
O=pathlib.Path(os.environ['ASSISTED_TEST_OUT']);capture=O/'captures';capture.mkdir(exist_ok=True)
source=(R/'test/browser_assisted/client.lua').read_text()
changes=[("deliver7({v=1,error='too_large',retryable=false},413);","deliver7({v=1,error='too_large',retryable=false},413);capture('image-error-portrait');"),("menu('Mode:');click('Offline')","menu('Mode:');capture('modes-portrait');click('Offline');capture('offline-portrait')"),("click('GET search form');click('q:')","click('GET search form');capture('form-portrait');click('q:')"),("click('Image');on_update();assert(endpoint('image')","capture('image-placeholder-portrait');click('Image');on_update();assert(endpoint('image')"),("w,h=800,480;draw();capture('assisted-opening-landscape');w,h=480,800;draw()","w,h=800,480;draw();capture('assisted-opening-landscape');menu('Mode:');capture('modes-landscape');on_button('back',true);draw();w,h=480,800;draw()")]
for old,new in changes:assert old in source;source=source.replace(old,new,1)
# Capture the mode menu, with no retired configuration-import action.
needle="if b[6]:sub(1,#label)==label then on_tap"
assert needle in source;source=source.replace(needle,"if b[6]:sub(1,#label)==label then if label=='Mode:'then capture('mode-menu-'..(w==480 and 'portrait'or 'landscape'))end;on_tap",1)
driver=O/'capture-extra.lua';driver.write_text(source)
env=dict(os.environ,BROWSER_CAPTURE=str(capture),ASSISTED_PNG_FIXTURE=str(O/'palette.png'),ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
subprocess.run([str(O/'font-lua'),str(driver)],env=env,check=True)
def chunk(k,b):return struct.pack('>I',len(b))+k+b+struct.pack('>I',zlib.crc32(k+b))
images=[]
for p in sorted(capture.glob('*.pgm')):
 magic,dims,depth,data=p.read_bytes().split(b'\n',3);w,h=map(int,dims.split());assert magic==b'P5'and depth==b'255'and len(data)==w*h
 png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+data[y*w:(y+1)*w]for y in range(h))))+chunk(b'IEND',b'')
 out=p.with_suffix('.png');out.write_bytes(png);images.append({'file':out.name,'width':w,'height':h,'sha256':hashlib.sha256(png).hexdigest()})
inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in [R/'test/browser_assisted/client.lua',R/'test/browser/font_host.cpp',driver,*sorted((R/'apps/browser').glob('*.lua'))]}
(capture/'manifest.json').write_text(json.dumps({'kind':'original synthetic fixtures, production EpdFont metrics/bitmaps; host canvas, not physical panel','inputs':inputs,'images':images},indent=2)+'\n')
