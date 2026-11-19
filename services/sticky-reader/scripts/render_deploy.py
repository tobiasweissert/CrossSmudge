#!/usr/bin/env python3
import json,pathlib,re,sys
if len(sys.argv)!=2 or not re.fullmatch(r'localhost/sticky-reader@sha256:[0-9a-f]{64}',sys.argv[1]):raise SystemExit('usage: render_deploy.py localhost/sticky-reader@sha256:<reviewed manifest digest>')
p=pathlib.Path(__file__).resolve().parent.parent/'deploy/app.template.json'
print(p.read_text().replace('IMAGE_DIGEST_REQUIRED',sys.argv[1]),end='')
