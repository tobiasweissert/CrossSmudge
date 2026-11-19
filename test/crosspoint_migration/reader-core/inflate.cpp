#include <InflateReader.h>
#include <zlib.h>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
static std::vector<uint8_t> pack(const std::vector<uint8_t>&in){z_stream z{};assert(deflateInit2(&z,6,Z_DEFLATED,-15,8,Z_DEFAULT_STRATEGY)==Z_OK);std::vector<uint8_t>out(deflateBound(&z,in.size()));z.next_in=const_cast<uint8_t*>(in.data());z.avail_in=in.size();z.next_out=out.data();z.avail_out=out.size();assert(deflate(&z,Z_FINISH)==Z_STREAM_END);out.resize(z.total_out);deflateEnd(&z);return out;}
struct Ctx{InflateReader reader;const uint8_t*src;size_t remaining;};
static int feed(uzlib_uncomp*u){auto*c=reinterpret_cast<Ctx*>(u);if(!c->remaining)return -1;size_t n=std::min(size_t(123),c->remaining);auto*p=c->src;c->src+=n;c->remaining-=n;u->source=p+1;u->source_limit=p+n;return p[0];}
int main(){
 std::vector<uint8_t>plain(100000);for(size_t i=0;i<plain.size();++i)plain[i]=(i*37+i/151)%251;auto packed=pack(plain);
 for(bool segmented:{false,true}){Ctx c;c.src=packed.data();c.remaining=packed.size();std::array<std::array<uint8_t,InflateReader::RING_SEGMENT_BYTES>,InflateReader::RING_SEGMENTS>owned{};uint8_t*segments[InflateReader::RING_SEGMENTS];for(size_t i=0;i<InflateReader::RING_SEGMENTS;++i)segments[i]=owned[i].data();if(segmented)assert(c.reader.initWithSegments(segments));else assert(c.reader.init(true));c.reader.setReadCallback(feed);std::vector<uint8_t>out(plain.size());for(size_t i=0;i<plain.size();i+=127)assert(c.reader.read(out.data()+i,std::min(size_t(127),plain.size()-i)));assert(out==plain);}
 for(int delta:{-1,0,1}){InflateReader r;assert(r.init());r.setSource(packed.data(),packed.size());std::vector<uint8_t>out(plain.size()+2);assert(r.readExact(out.data(),plain.size()+delta)==(delta==0));}
 auto trailing=packed;trailing.push_back(0);{InflateReader r;assert(r.init());r.setSource(trailing.data(),trailing.size());std::vector<uint8_t>out(plain.size()+1);assert(!r.readExact(out.data(),plain.size()));}
 uint8_t*missing[InflateReader::RING_SEGMENTS]={};InflateReader r;assert(!r.initWithSegments(missing));
 std::cout<<"PASS production uzlib contiguous/segmented streaming and strict one-shot completion\n";
}
