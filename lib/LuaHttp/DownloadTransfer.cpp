#include "DownloadTransfer.h"
namespace ink::http {
void exchangeDownload(Job& job, Io& io, DownloadFiles& files) {
  const uint32_t start=io.millis(io.context);
  DownloadStore store(files);
  if(!store.begin()){job.result.error="file_write";return;}
  BodySink sink{&store,job.maxBytes,[](void* p,const char* bytes,size_t n) {
    return static_cast<DownloadStore*>(p)->append(bytes,n);
  }};
  exchange(job.request,job.result,job.scratch,io,job.cancel,&sink);
  if(job.cancel.load())job.result.error="cancelled";
  if(!job.result.error) {
    struct Deadline {Job& job;Io& io;uint32_t start;} deadline{job,io,start};
    auto permit=[](void* p) {
      auto& d=*static_cast<Deadline*>(p);
      if(uint32_t(d.io.millis(d.io.context)-d.start)>=d.job.request.timeoutMs) {
        d.job.result.error="timeout";return false;
      }
      return true;
    };
    if(store.commit(job.cancel,permit,&deadline))job.committed=true;
    else if(!job.result.error)job.result.error=job.cancel.load()?"cancelled":"file_commit";
  }
  if(job.result.error)job.result.size=0;
}
}
