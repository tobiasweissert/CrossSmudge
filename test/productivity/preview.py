#!/usr/bin/env python3
"""Approximate Lua drawing previews (system fonts, not firmware screenshots)."""
import argparse, os, pathlib, subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
APPS = ('flashcards', 'habits', 'pomodoro', 'weather', 'browser')

def render(path, width, height):
    image = Image.new('RGB', (width, height), 'white')
    draw = ImageDraw.Draw(image)
    for line in path.read_text().splitlines():
        kind, *v = line.split('\t')
        if kind == 'text':
            x, y = map(float, v[:2]); text, font, style, align, color = v[2:]
            size = {'small': 16, 'ui12': 23, 'bitter14': 27}.get(font, 23)
            family = '/System/Library/Fonts/Supplemental/Arial' + (' Bold.ttf' if style == 'bold' else '.ttf')
            try:
                face = ImageFont.truetype(family, size)
            except OSError:
                face = ImageFont.truetype('DejaVuSans-Bold.ttf' if style == 'bold' else 'DejaVuSans.ttf', size)
            tw = draw.textlength(text, font=face)
            x -= tw / 2 if align == 'center' else tw if align == 'right' else 0
            draw.text((x, y), text, font=face, fill='black' if color == 'true' else 'white', anchor='lt')
        elif kind in ('rect', 'dither'):
            x, y, w, h = map(float, v[:4]); box = (round(x), round(y), round(x+w), round(y+h))
            if kind == 'dither':
                draw.rectangle(box, fill='#aaa'); continue
            r, fill, stroke, color = v[4:]; ink = 'black' if color == 'true' else 'white'
            draw.rounded_rectangle(box, radius=float(r), fill=ink if fill == 'true' else None,
                                   outline=None if fill == 'true' else ink, width=max(1, round(float(stroke))))
        elif kind == 'line':
            draw.line(tuple(map(float, v[:4])), fill='black' if v[5] == 'true' else 'white', width=max(1,round(float(v[4]))))
        elif kind == 'circle':
            x,y,r = map(float,v[:3]); draw.ellipse((x-r,y-r,x+r,y+r), fill='black' if v[3]=='true' else None, outline='black',width=max(1,round(float(v[4]))))
    return image

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=pathlib.Path,default=pathlib.Path('/tmp/crosssmudge-productivity-previews'))
    parser.add_argument('--runner',type=pathlib.Path,default=pathlib.Path('/tmp/productivity-preview-lua'))
    args=parser.parse_args();os.chdir(ROOT);args.out.mkdir(parents=True,exist_ok=True)
    subprocess.run(['cc','-O1','-Ilib/Lua/src','test/productivity/runner.c',*map(str,sorted(pathlib.Path('lib/Lua/src').glob('l*.c'))),'-lm','-o',str(args.runner)],check=True)
    for width,height in ((480,800),(800,480)):
        tiles=[]
        for app in APPS:
            capture=args.out/f'{app}-{width}.tsv'
            env=dict(os.environ,APP=app,WIDTH=str(width),HEIGHT=str(height),PREVIEW='main',CAPTURE=str(capture),LUA_APP_LIMIT='2097152',SMUDGE_X3_CONSTRAINTS='1')
            subprocess.run([str(args.runner),'test/productivity/host.lua'],env=env,check=True,stdout=subprocess.DEVNULL)
            img=render(capture,width,height);img.save(args.out/f'{app}-{width}.png');tiles.append(img)
        sheet=Image.new('RGB',(5*(width//2+12),height//2+20),'#ddd')
        for i,img in enumerate(tiles):sheet.paste(img.resize((width//2,height//2)),(i*(width//2+12)+6,10))
        sheet.save(args.out/f'overview-{width}.png')
    print(args.out)

if __name__=='__main__':main()
