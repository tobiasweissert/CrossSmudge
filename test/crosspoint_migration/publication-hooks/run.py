#!/usr/bin/env python3
"""Production-body execution over explicit hardware doubles; not physical FAT/target qualification."""
import argparse, hashlib, json, pathlib, subprocess
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3]; out=pathlib.Path(a.evidence);out.mkdir(parents=True,exist_ok=True)
def method(text, signature):
    start=text.index(signature); begin=text.index('{',start); depth=1;i=begin+1
    # Production methods here contain no unmatched braces in string literals/comments.
    while depth:
        depth += (text[i]=='{')-(text[i]=='}');i+=1
    return text[start:i]
paths=['src/network/WebDAVHandler.cpp','src/util/BookCacheUtils.cpp','lib/AirDrop/port/ReceiverEsp32.cpp']
src={x:(root/x).read_text() for x in paths}
web=src[paths[0]];cache=src[paths[1]];air=src[paths[2]]
parts=[(pathlib.Path(__file__).with_name('doubles.cpp')).read_text()]
parts += [method(web,s) for s in ['void WebDAVHandler::raw(', 'void WebDAVHandler::handlePut(', 'void WebDAVHandler::handleDelete(', 'void WebDAVHandler::handleMove(', 'void WebDAVHandler::handleCopy(']]
parts += ['namespace cacheprobe { bool clearBookCachePreservingUserState(const std::string&);', '\n'.join(l for l in cache.splitlines() if not l.startswith('#include')), '}']
parts += ['namespace airprobe {', (pathlib.Path(__file__).with_name('air-double.cpp')).read_text(),method(air,'bool finishFile('),'}', (pathlib.Path(__file__).with_name('cases.cpp')).read_text()]
code='\n'.join(parts);(out/'probe.cpp').write_text(code)
# Restrict source shape as well as runtime effects; no raw cert contents are read.
assert '#include <LibraryBuilder.h>' in air
assert 'ClippingStore::moveBook' not in method(web,'void WebDAVHandler::handleMove(')
assert air.count('library::markLibraryIndexDirty()')==1
assert air.index('library::markLibraryIndexDirty()')>air.index('portEXIT_CRITICAL(&statusMux);',air.index('bool finishFile('))
for arr in ['EPUB_USER_STATE_FILES','PAGE_PROGRESS_FILES','CACHE_CLEAR_USER_STATE_FILES']:
    assert '"reading_stats_off.tmp"' in cache[cache.index(arr):cache.index('};',cache.index(arr))]
cmd=['g++','-std=c++17','-O0','-g0','-Wall','-Wextra',str(out/'probe.cpp'),'-o',str(out/'probe')]
if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100);(out/'compile.log').write_text(r.stdout)
receipt={'kind':'actual extracted production methods and cache bodies / explicit hardware doubles','source_sha256':{x:hashlib.sha256((root/x).read_bytes()).hexdigest() for x in paths},'command':cmd,'compile_exit':r.returncode,'sanitize':a.sanitize}
if r.returncode==0:
    r=subprocess.run([str(out/'probe')],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=20);(out/'run.log').write_text(r.stdout);receipt['run_exit']=r.returncode;print(r.stdout)
(out/'receipt.json').write_text(json.dumps(receipt,indent=2));raise SystemExit(r.returncode)
