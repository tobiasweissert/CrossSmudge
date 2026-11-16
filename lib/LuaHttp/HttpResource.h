#pragma once
#include <cstddef>
#include <cstdint>
namespace ink::http {
// An immutable independently named spool. No rename is used. Ownership moves
// from worker job to its session only after finish; release removes the spool.
void cleanupResources(const char* root);
struct Resource {
 char path[384]{};
 void* writer=nullptr;
 size_t size=0;
 uint32_t hash=1;
 bool complete=false;
 Resource()=default;
 ~Resource(){reset();}
 Resource(const Resource&)=delete;
 Resource& operator=(const Resource&)=delete;
 bool begin(const char* root,int id);
 bool append(const char* bytes,size_t count);
 bool finish();
 size_t read(size_t offset,char* bytes,size_t count)const;
 bool copy(const char* destination)const;
 void take(Resource& other);
 void reset();
};
}
