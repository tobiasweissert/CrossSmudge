#!/usr/bin/env python3
"""Actual worker inventory + HalStorageSink methods + StorageSink, with checked memory I/O."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--output',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[2]
def method(text,marker):
 start=text.index(marker);i=text.index('{',start);depth=1;j=i+1
 while depth:
  depth+=(text[j]=='{')-(text[j]=='}');j+=1
 return text[start:j]
hal=(r/'lib/AirDrop/storage/HalStorageSink.cpp').read_text();worker=(r/'lib/AirDrop/port/ReceiverEsp32.cpp').read_text();start=worker.index('  if(stop.load(std::memory_order_acquire))goto cleanup;',worker.index('void worker(void* p)'));end=worker.index('  if(!HalAirDropRadio::releaseArduino())',start)
cpp=r'''
#define main previous_storage_main
#include "storage_test.cpp"
#undef main
#include <atomic>
#define LOG_ERR(...) ((void)0)
static unsigned nonceByte=9;
void esp_fill_random(void* p,size_t n){std::memset(p,nonceByte,n);}
namespace airdrop::storage {
class HalStorageSink {
 StorageSink sink_;
 public:
 std::vector<std::string> tokens;
 explicit HalStorageSink(Files& f):sink_(f){}
 void abortTransfer();bool begin(const char*,uint32_t);bool logged(bool);State inspect(const char*);State inspectJournal(const char*);
 bool append(const uint8_t*p,size_t n){return sink_.append(p,n);}bool finish(){return sink_.finish();}
 bool recoveryToken(unsigned i,char token[33],bool&ok){ok=true;if(i>=tokens.size())return false;std::memcpy(token,tokens[i].c_str(),33);return true;}
};
'''+ '\n'.join(method(hal,m)for m in ['void HalStorageSink::abortTransfer(', 'bool HalStorageSink::logged(', 'bool HalStorageSink::begin(', 'State HalStorageSink::inspect(', 'State HalStorageSink::inspectJournal('])+r'''
}
namespace storage=airdrop::storage;
std::atomic<bool> stop{false};struct {bool publicationUncertain=false;} status;int statusMux;
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
struct Context{storage::HalStorageSink storage;bool failed=false;explicit Context(Files&f):storage(f){}};
void failure(Context&c,const char*){c.failed=true;}
bool inventory(Context&c){
'''+worker[start:end]+r'''
 return true;
cleanup:return false;
}
int main(){
 MemoryFiles files;files.files["/apps/save.dat"]={'o','l','d'};
 // Completed entry first; a retained inactive phase-1 is enumerated LAST.
 uint8_t complete[16]={7};
 {StorageSink old(files);assert(old.begin("complete.txt",3,complete));assert(write(old,3));assert(old.finish());}
 {StorageSink old(files);assert(old.begin("partial.txt",3,token));assert(write(old,2));old.cancel();}
 auto retained=files.files;
 Context c(files);c.storage.tokens={"07000000000000000000000000000000","01020304050000000000000000000000"};
 assert(inventory(c)&&!c.failed&&!status.publicationUncertain);
 assert(files.files==retained);
 // Same recovered production owner receives a DIFFERENT freshly consented nonce.
 if(!c.storage.begin("new.txt",3)){std::fputs("FAIL AD-STORAGE-001: inactive phase1 blocked new consented nonce after actual worker recovery/HalStorageSink::begin\n",stderr);return 1;}
 const uint8_t bytes[]={'n','e','w'};assert(c.storage.append(bytes,3));assert(c.storage.finish());
 for(const auto&[path,data]:retained)assert(files.files.at(path)==data);
 // A live writer still cannot be reused. No second nonce may be created.
 MemoryFiles live;Context active(live);assert(active.storage.begin("live.txt",3));auto before=live.files;
 assert(!active.storage.begin("other.txt",3));assert(live.files==before);
 assert(active.storage.append(bytes,3));assert(active.storage.finish());
 MemoryFiles coreLive;StorageSink core(coreLive);assert(core.begin("core.txt",3,token));
 assert(!core.begin("other.txt",3,complete));assert(core.state()==State::Receiving);assert(core.append(bytes,3));assert(core.finish());
 // A real alias uncertainty remains latched; inspecting good history cannot clear it.
 MemoryFiles uncertain;{StorageSink old(uncertain);assert(old.begin("bad.txt",3,token));assert(write(old,3));uncertain.aliasFalse=true;assert(!old.finish());}
 auto aliases=uncertain.files;StorageSink fresh(uncertain);assert(fresh.inspect("01020304050000000000000000000000")==State::Uncertain);
 assert(!fresh.begin("next.txt",3,complete));assert(!fresh.begin("next.txt",3,complete));
 assert(fresh.inspect("07000000000000000000000000000000")==State::Uncertain);assert(uncertain.files==aliases);
 // An interrupted connection closes its stage without cancelling the whole owner.
 MemoryFiles interrupted;Context retry(interrupted);assert(retry.storage.begin("first.bin",3));
 assert(retry.storage.append(bytes,1));retry.storage.abortTransfer();auto partial=interrupted.files;
 nonceByte=10;assert(retry.storage.begin("second.bin",3));assert(retry.storage.append(bytes,3));assert(retry.storage.finish());
 for(const auto&[path,data]:partial)assert(interrupted.files.at(path)==data);
 std::puts("PASS actual worker + HalStorageSink/core recovery: phase1 last, fresh nonce publish, old bytes preserved, live writer and uncertainty refused");
}
'''
with tempfile.TemporaryDirectory(prefix='airdrop-recovery-ownership-')as tmp:
 out=a.output or Path(tmp);out.mkdir(parents=True,exist_ok=True);(out/'probe.cpp').write_text(cpp)
 flags=['-O1','-g','-Wno-deprecated-declarations']
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 cmds=[['cc',*flags,'-Ilib/miniz/src','-c','lib/miniz/src/miniz_impl.c','-o',str(out/'miniz.o')],['c++','-std=c++20',*flags,'-Itest/airdrop','-Ilib/AirDrop/storage','-Ilib/miniz/src',str(out/'probe.cpp'),'lib/AirDrop/storage/StorageSink.cpp','lib/AirDrop/stream/StreamDecoder.cpp',str(out/'miniz.o'),'-lcrypto','-lz','-o',str(out/'probe')],[str(out/'probe')]]
 results=[]
 for i,cmd in enumerate(cmds):
  run=subprocess.run(cmd,cwd=r,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));(out/f'{i}.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='');results.append({'command':cmd,'exit':run.returncode})
  (out/'receipt.json').write_text(json.dumps({'commands':results,'worker_fragment_sha256':hashlib.sha256(worker[start:end].encode()).hexdigest(),'hal_sha256':hashlib.sha256(hal.encode()).hexdigest(),'sanitize':a.sanitize},indent=2)+'\n')
  if run.returncode:raise SystemExit(1)
