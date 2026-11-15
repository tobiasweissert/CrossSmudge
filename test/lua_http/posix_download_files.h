#pragma once
// Native Linux integration adapter, NOT an SD/FAT atomicity claim.
// Trusted test-owned root opened component-by-component without symlink traversal.
#include "DownloadStore.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
class PosixDownloadFiles final : public ink::http::DownloadFiles {
 public:
  explicit PosixDownloadFiles(const char* path) {
    if(!path || *path!='/')return;
    root_=::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    ++path;
    while(*path && root_>=0) {
      const char* end=strchr(path,'/');size_t n=end?size_t(end-path):strlen(path);
      if(!n || n>255){::close(root_);root_=-1;break;}
      char part[256];memcpy(part,path,n);part[n]=0;
      if(!strcmp(part,".") || !strcmp(part,"..")){::close(root_);root_=-1;break;}
      int next=openat(root_,part,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
      ::close(root_);root_=next;if(!end)break;path=end+1;
    }
  }
  ~PosixDownloadFiles() override {close();if(root_>=0)::close(root_);}
  bool prepare() override {return root_>=0;}
  int exists(const char* leaf) override {
    if(root_<0 || !valid(leaf))return -1;
    struct stat st{};
    if(fstatat(root_,leaf,&st,AT_SYMLINK_NOFOLLOW)==0)return S_ISREG(st.st_mode)?1:-1;
    return errno==ENOENT?0:-1;
  }
  bool open(const char* leaf,bool create) override {
    if(root_<0 || !valid(leaf) || fd_>=0)return false;
    fd_=openat(root_,leaf,(create?O_WRONLY|O_CREAT|O_EXCL:O_RDONLY)|O_NOFOLLOW|O_CLOEXEC,0600);
    struct stat st{};
    if(fd_>=0 && (fstat(fd_,&st)!=0 || !S_ISREG(st.st_mode))){close();return false;}
    return fd_>=0;
  }
  size_t size() override {struct stat st{};return fstat(fd_,&st)==0?size_t(st.st_size):SIZE_MAX;}
  bool seek(size_t n) override {return lseek(fd_,n,SEEK_SET)==off_t(n);}
  int read(char* p,size_t n) override {return ::read(fd_,p,n);}
  size_t write(const char* p,size_t n) override {auto w=::write(fd_,p,n);return w<0?0:size_t(w);}
  bool sync() override {return fsync(fd_)==0;}
  bool close() override {if(fd_<0)return true;int fd=fd_;fd_=-1;return ::close(fd)==0;}
  bool remove(const char* leaf) override {
    if(root_<0 || !valid(leaf))return false;
    return unlinkat(root_,leaf,0)==0 || errno==ENOENT;
  }
  bool promote(const char* leaf) override {
    return root_>=0 && valid(leaf) && strcmp(leaf,"part") &&
      syscall(SYS_renameat2,root_,"part",root_,leaf,RENAME_NOREPLACE)==0;
  }
 private:
  static bool valid(const char* p) {return !strcmp(p,"0") || !strcmp(p,"1") || !strcmp(p,"part");}
  int root_=-1,fd_=-1;
};
