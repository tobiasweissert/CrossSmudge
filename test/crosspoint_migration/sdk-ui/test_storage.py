#!/usr/bin/env python3
"""Execute extracted production bounded-read method over explicit IO doubles."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
def method(text, anchor):
    start=text.index(anchor);b=text.index('{',start);depth=0
    for i in range(b,len(text)):
        if text[i]=='{':depth+=1
        if text[i]=='}':
            depth-=1
            if depth==0:return text[start:i+1]
    raise AssertionError(anchor)
def main():
    body=method((ROOT/'lib/hal/HalStorage.cpp').read_text(),'bool HalStorage::readFileToString(')
    with tempfile.TemporaryDirectory(prefix='sdk-ui-storage-',dir=os.environ['TMPDIR']) as td:
        p=Path(td)
        (p/'probe.cpp').write_text(r'''
#include <cassert>
#include <cstddef>
#include <climits>
#include <cstring>
#include <string>
#include <limits>
#define LOG_ERR(...) ((void)0)
constexpr int MALLOC_CAP_8BIT=1;
size_t largest=4096;
size_t heap_caps_get_largest_free_block(int){return largest;}
bool available=true,directory=false,closedOk=true; int nread=3; size_t fileSize=3;int closes=0;
struct HalFile {
 ~HalFile(){if(!closed) close();} bool closed=false;
 bool isDirectory(){return directory;} size_t fileSize(){return ::fileSize;}
 int read(void* out,size_t n){std::memset(out,'x',n);return nread;}
 bool close(){closed=true;++closes;return closedOk;}
};
struct HalStorage {bool openFileForRead(const char*,const std::string&,HalFile&){return available;}
 bool readFileToString(const char*,const std::string&,size_t,std::string&);
};
'''+body+r'''
int main(){HalStorage s; std::string out="old";
 assert(s.readFileToString("t","/a",10,out)&&out=="xxx"&&closes==1);
 nread=2; assert(!s.readFileToString("t","/a",10,out)&&out.empty());
 nread=3;closedOk=false;assert(!s.readFileToString("t","/a",10,out)&&out.empty());
 closedOk=true;largest=514;assert(!s.readFileToString("t","/a",10,out)&&out.empty());
 largest=4096;assert(!s.readFileToString("t","/a",2,out)&&out.empty());
 directory=true;assert(!s.readFileToString("t","/a",10,out)&&out.empty());
 directory=false;fileSize=0;assert(!s.readFileToString("t","/a",10,out)&&out.empty());
 fileSize=3;available=false;assert(!s.readFileToString("t","/a",10,out)&&out.empty());
}
''')
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti',str(p/'probe.cpp'),'-o',str(p/'probe')]
        if os.environ.get('SANITIZE')=='1':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);subprocess.run([str(p/'probe')],check=True)
        print('PASS: production bounded read success, short read, checked close, heap, cap, directory, empty, missing')
if __name__=='__main__':main()
