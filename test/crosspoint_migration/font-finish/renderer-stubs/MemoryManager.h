#pragma once
#include <functional>
#include <cstddef>
namespace freeink {
class MemoryManager {public:
 struct Sink {const char* name;int priority;std::function<size_t(size_t)> release;};
 static MemoryManager& instance(){static MemoryManager m;return m;}
 void registerSink(Sink){} // No asynchronous eviction in this deterministic raster fixture.
 size_t freeBytes()const{return 1024*1024;}
 bool ensureFree(size_t){return true;}
};
}
