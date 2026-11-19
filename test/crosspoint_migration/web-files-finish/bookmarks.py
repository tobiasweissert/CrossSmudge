#!/usr/bin/env python3
"""Actual retained bookmark codecs/preparation over the existing explicit SD double."""
from pathlib import Path
import ast,argparse,hashlib,json,subprocess
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.evidence;O.mkdir(parents=True,exist_ok=True)
old=R/'test/file_actions/nested_run.py'
preamble=next(ast.literal_eval(n.value) for n in ast.parse(old.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='preamble' for t in n.targets))
def method(sig):
 s=(R/'src/BookmarkStore.cpp').read_text();i=s.index(sig);return s[i:s.index('\n}',i)+2]
source=preamble+'\nusing HalFile=FsFile; // Host-only spelling bridge for real modern Serialization helpers.\n'+(R/'lib/Serialization/Serialization.h').read_text().replace('#pragma once','').replace('#include <HalStorage.h>','')
for sig in ['bool readBookmarkString(', 'bool readBookmarkCount(', 'bool BookmarkStore::readFromFile(const std::string&', 'bool bookmarksMatchIdentity(', 'bool mergeBookmarks(', 'bool deleteBookmarkStorePath(', 'bool BookmarkStore::writeToFile(', 'bool BookmarkStore::beginRenameMigration(', 'bool BookmarkStore::commitRenameMigration(', 'bool BookmarkStore::rollbackRenameMigration(']:source+='\n'+method(sig)
source+=r'''
void setup(){disk.clear();failWriteAt=0;writeCalls=0;failSync=false;shortWrite=false;failRead=false;failBackup=false;corruptRead=false;mutations=0;BookmarkStore w;w.bookFilePath="/Source/A.epub";w.bookTitle="title";w.bookAuthor="author";w.storeFilePath="/current/Source/A.epub";Bookmark b{};b.spineIndex=3;b.progress=0.5f;strcpy(b.snippet,"saved UTF-8 text");w.bookmarks.push_back(b);assert(w.writeToFile());writeCalls=0;}
int main(){BookmarkStore::RenameMigration m;setup();assert(BookmarkStore::beginRenameMigration("/Source/A.epub","/Target/A.epub","title","author","epub",m,true));int boundaries=writeCalls;assert(BookmarkStore::commitRenameMigration(m));assert(!disk.count("/current/Source/A.epub"));BookmarkStore reader;reader.bookFilePath="/Target/A.epub";std::vector<Bookmark> out;bool rewrite=false;assert(reader.readFromFile("/current/Target/A.epub",out,rewrite));assert(out.size()==1&&out[0].spineIndex==3&&std::string(out[0].snippet)=="saved UTF-8 text");
for(int i=1;i<=boundaries;++i){setup();const auto original=disk.at("/current/Source/A.epub");failWriteAt=i;assert(!BookmarkStore::beginRenameMigration("/Source/A.epub","/Target/A.epub","title","author","epub",m,true));assert(m.uncertain);auto retained=disk;assert(!BookmarkStore::rollbackRenameMigration(m)&&!BookmarkStore::commitRenameMigration(m));assert(retained==disk&&disk.at("/current/Source/A.epub")==original);}
setup();failSync=true;assert(!BookmarkStore::beginRenameMigration("/Source/A.epub","/Target/A.epub","title","author","epub",m,true)&&m.uncertain);
setup();disk["/current/Target/A.epub.rename.bak"]="retained";auto before=disk;assert(!BookmarkStore::beginRenameMigration("/Source/A.epub","/Target/A.epub","title","author","epub",m,true)&&m.uncertain&&before==disk);
std::cout<<"PASS real bookmark writer/prepare/commit: "<<boundaries<<" short-write boundaries, sync uncertainty, no rollback/commit after uncertainty, retained backup refusal\n";
}
'''
(O/'bookmarks.cpp').write_text(source)
cmd=['g++','-std=c++20','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-parameter','-fno-exceptions','-fno-rtti','-I'+str(R),str(O/'bookmarks.cpp'),'-o',str(O/'bookmarks')]
if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
r=subprocess.run(cmd,capture_output=True,text=True,timeout=70);log=r.stdout+r.stderr;rc=r.returncode
if not rc:
 r=subprocess.run([str(O/'bookmarks')],capture_output=True,text=True,timeout=20);log+=r.stdout+r.stderr;rc=r.returncode
(O/'bookmarks.log').write_text(log);print(log)
inputs=[R/'src/BookmarkStore.cpp',R/'src/BookmarkStore.h',R/'lib/Serialization/Serialization.h',old,Path(__file__)]
(O/'bookmarks.json').write_text(json.dumps({'returncode':rc,'command':cmd,'coverage':'extracted actual bookmark methods/codec with supplied FsFile double and path-identity adapter','inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2));raise SystemExit(rc)
