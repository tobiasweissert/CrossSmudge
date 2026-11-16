"""Repack original sentence phrases only, without altering the64-case bank."""
from pathlib import Path
import subprocess,re,json,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[2];revision='d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b';path='src/apps_local/murdle/MurdleCast.cpp';raw=subprocess.check_output(['git','show',revision+':'+path],cwd=a.reference);text=raw.decode();out=[]
for name in ['kWeapons','kPlaces','kMotives']:
 block=text.split(' '+name+'[')[1].split('};',1)[0];pairs=re.findall(r'\{"([^"\n]+)",\s*"([^"\n]+)"',block);assert pairs
 out.append('{'+','.join('['+json.dumps(k)+']='+json.dumps(v)for k,v in pairs)+'}')
data=('return {'+','.join(out)+'}\n').encode();dest=root/'apps/cp-murdle/phrases.lua';dest.write_bytes(data);(root/'apps/cp-murdle/art/phrases-source.json').write_text(json.dumps(dict(revision=revision,path=path,source_sha256=hashlib.sha256(raw).hexdigest(),output_sha256=hashlib.sha256(data).hexdigest(),license='LICENSE-CrossPlay.txt'),indent=2)+'\n')
