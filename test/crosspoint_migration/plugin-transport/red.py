"""Baseline regression: actual inherited openClient must not select insecure TLS."""
from pathlib import Path
import subprocess, os
root=Path(__file__).resolve().parents[3]
e=Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/plugin-transport'); e.mkdir(parents=True,exist_ok=True)
s=subprocess.check_output(['git','-C',str(root),'show','9ac3475733e677b7f61ba3f6e8ce41ce1b9b2967:src/util/PluginHttp.cpp'],text=True); section=s[s.index('freeink::SecureHttpClient* openClient'):s.index('// A chunked body')]
prefix='''#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <cassert>
namespace pluginhttp {using Headers=std::vector<std::pair<std::string,std::string>>; std::string escapeUrl(std::string s){return s;}}
namespace freeink {struct SecureHttpClient {bool insecure=false; void setUserAgent(const char*){} void setInsecure(){insecure=true;} void setTimeout(int){} bool begin(std::string){return true;} bool connect(std::function<bool()>){return true;} void addHeader(std::string,std::string){} };}
'''
f=e/'baseline.cpp'; f.write_text(prefix+section+'int main(){freeink::SecureHttpClient tmp; openClient(nullptr,tmp,"https://public.example/",{}); assert(!tmp.insecure && "production selected insecure TLS");}\n')
exe=e/'baseline'; subprocess.run(['c++','-std=c++17',str(f),'-o',str(exe)],check=True,timeout=60)
r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10); (e/'baseline-red.log').write_text(r.stdout+r.stderr+f'\nexit={r.returncode}\n');print(r.stderr);assert r.returncode!=0
