#include <HalStorage.h>
#include <GfxRenderer.h>
#include <BuildScratch.h>
#include <InflateStream.h>
#include <ZipFile.h>
#include <Epub/converters/JpegToFramebufferConverter.h>
#include <Epub/converters/PngToFramebufferConverter.h>
#include <Epub/converters/ImageDimsProbe.h>
#include <PngToBmpConverter.h>
#include <JpegToBmpConverter.h>
#include <zlib.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <array>
static int cases=0;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::abort();}}while(0)
struct Sink:Print {std::vector<uint8_t> b;size_t limit=SIZE_MAX; size_t write(uint8_t x)override{return write(&x,1);}size_t write(const uint8_t*p,size_t n)override{n=std::min(n,limit-b.size());b.insert(b.end(),p,p+n);return n;}};
static std::vector<uint8_t> load(const std::string&path){std::ifstream f(path,std::ios::binary);CHECK(f);return {(std::istreambuf_iterator<char>(f)),{}};}
static std::vector<uint8_t> pack(const std::vector<uint8_t>&in){z_stream z{};CHECK(deflateInit2(&z,6,Z_DEFLATED,-15,8,Z_DEFAULT_STRATEGY)==Z_OK);std::vector<uint8_t>out(deflateBound(&z,in.size()));z.next_in=const_cast<uint8_t*>(in.data());z.avail_in=in.size();z.next_out=out.data();z.avail_out=out.size();CHECK(deflate(&z,Z_FINISH)==Z_STREAM_END);out.resize(z.total_out);deflateEnd(&z);return out;}
static void le(std::vector<uint8_t>&b,uint32_t v,int n){while(n--){b.push_back(v);v>>=8;}}
static std::vector<uint8_t> zip(const std::vector<uint8_t>&raw,uint32_t size,bool deflated){
 std::vector<uint8_t>b;le(b,0x04034b50,4);le(b,20,2);le(b,0,2);le(b,deflated?8:0,2);le(b,0,4);le(b,0,4);le(b,raw.size(),4);le(b,size,4);le(b,1,2);le(b,0,2);b.push_back('x');b.insert(b.end(),raw.begin(),raw.end());auto start=b.size();
 le(b,0x02014b50,4);le(b,20,2);le(b,20,2);le(b,0,2);le(b,deflated?8:0,2);le(b,0,4);le(b,0,4);le(b,raw.size(),4);le(b,size,4);le(b,1,2);le(b,0,2);le(b,0,2);le(b,0,2);le(b,0,2);le(b,0,4);le(b,0,4);b.push_back('x');auto count=b.size()-start;
 le(b,0x06054b50,4);le(b,0,2);le(b,0,2);le(b,1,2);le(b,1,2);le(b,count,4);le(b,start,4);le(b,0,2);return b;
}
static void inflateTests(){
 std::vector<uint8_t>plain;for(int i=0;i<100000;++i)plain.push_back((i*37+i/151)%251);auto packed=pack(plain);
 for(bool scratch:{false,true}){
  alignas(8) std::array<uint8_t,48000>loan{};if(scratch)buildscratch::lend(loan.data(),loan.size());
  {InflateStream s;CHECK(s.init(true));CHECK(s.usesBuildScratch()==scratch);s.setSource(packed.data(),packed.size());std::vector<uint8_t>out;std::array<uint8_t,127>buf{};
   for(;;){size_t n=0;auto status=s.readAtMost(buf.data(),buf.size(),&n);out.insert(out.end(),buf.begin(),buf.begin()+n);CHECK(status!=InflateStream::Status::Error);if(status==InflateStream::Status::Done)break;}CHECK(out==plain);}
  if(scratch){CHECK(buildscratch::claim(loan.size())==loan.data());buildscratch::release(loan.data());buildscratch::reclaim();}++cases;
 }
 {InflateStream s;fakeheap::internal.fail=1;CHECK(!s.init(true));CHECK(fakeheap::live.empty());CHECK(s.init(true));}++cases;
 CHECK(fakeheap::live.empty());
}
static void zipTests(){
 const std::string path="/archive";std::vector<uint8_t>plain(1000,'a');auto compressed=pack(plain);
 for(bool deflated:{false,true})for(bool cooperative:{false,true}){
  Storage.put(path,zip(deflated?compressed:plain,plain.size(),deflated));ZipFile z(path);Sink sink;
  if(cooperative){auto r=z.openFileStream("x",17);CHECK(r);for(int i=0;;++i){CHECK(i<1000);auto state=r->pump(sink,29);CHECK(state!=ZipStreamStatus::Error);if(state==ZipStreamStatus::Done)break;}}
  else CHECK(z.readFileToStream("x",sink,17));CHECK(sink.b==plain);
  std::deque<ZipFile::SizeTarget> targets{{ZipFile::fnvHash64("x",1),1,0}};
  std::deque<uint32_t> sizes(1);CHECK(z.fillUncompressedSizes(targets,sizes)==1&&sizes[0]==plain.size());
  ++cases;
 }
 for(int delta:{-1,0,1}){
  Storage.put(path,zip(compressed,plain.size()+delta,true));ZipFile z(path);size_t n=SIZE_MAX;auto data=z.readFileToMemory("x",&n,true);CHECK((data!=nullptr)==(delta==0));if(data){CHECK(n==plain.size());CHECK(data[n]==0);free(data);}++cases;
 }
 // Missing terminal bytes must not become a successful exact-size prefix.
 for(size_t trim=1;trim<=3;++trim){auto truncated=compressed;truncated.resize(truncated.size()-trim);Storage.put(path,zip(truncated,plain.size(),true));ZipFile z(path);size_t n=0;CHECK(!z.readFileToMemory("x",&n));++cases;}
 Storage.put(path,zip(compressed,plain.size(),true));{ZipFile z(path);Sink s;s.limit=5;CHECK(z.readFileToStream("x",s,17,true));}++cases;
 {ZipFile z(path);Sink s;s.limit=5;CHECK(!z.readFileToStream("x",s,17,false));CHECK(!z.readFileToStream("x",s,0));}++cases;
 Storage.put(path,zip(pack({}),0,true));{ZipFile z(path);size_t n=9;auto d=z.readFileToMemory("x",&n);CHECK(d&&n==0);free(d);}++cases;
}
static void imageTests(const std::string&dir){
 PngToFramebufferConverter png;JpegToFramebufferConverter jpg;
 for(const char* name:{"mid.png","rgba.png","pal.png"}){
  Storage.put("/png",load(dir+"/"+name));ImageDimensions dims{};CHECK(png.getDimensions("/png",dims));CHECK(dims.width==64&&dims.height==16);CHECK(Storage.openHandles("/png")==0);
  GfxRenderer r(128,64);RenderConfig cfg{0,0,64,16};cfg.useMonochromeDither=true;CHECK(png.decodeToFramebuffer("/png",r,cfg));CHECK(Storage.openHandles("/png")==0);
  size_t black=0;for(int y=0;y<16;++y)for(int x=0;x<64;++x)black+=!(r.getWriteTarget()[y*16+x/8]&(0x80>>(x%8)));
  if(std::string(name)=="mid.png")CHECK(black>430&&black<570);else CHECK(black==0);
  cfg.cacheOnly=true;cfg.cachePath="/pixels";CHECK(png.decodeToCache("/png",cfg));CHECK(Storage.openHandles("/png")==0);++cases;
 }
 for(const char*name:{"rgba.png","grayalpha16.png","graykey16.png","rgbkey16.png","pal.png","graynotkey16.png"}){
  Storage.put("/source",load(dir+"/"+name));HalFile f;CHECK(Storage.openFileForRead("t","/source",f));Sink s;CHECK(PngToBmpConverter::pngFileTo1BitBmpStreamWithSize(f,s,64,16));CHECK(s.b.size()>62);
  bool white=true;for(size_t i=62;i<s.b.size();++i)white=white&&s.b[i]==255;
  CHECK(white==(std::string(name)!="graynotkey16.png"));++cases;
 }
 Storage.put("/bad",load(dir+"/baddepth.png"));{HalFile f;CHECK(Storage.openFileForRead("t","/bad",f));Sink s;CHECK(!PngToBmpConverter::pngFileTo1BitBmpStreamWithSize(f,s,64,16));CHECK(s.b.empty());}++cases;
 for(const char*suffix:{"jpg","progressive.jpg"}){
  Storage.put("/jpg",load(std::string("test/browser/fixtures/midtone.")+suffix));ImageDimensions dims{};CHECK(jpg.getDimensions("/jpg",dims));CHECK(Storage.openHandles("/jpg")==0);
  GfxRenderer r(800,480);RenderConfig cfg{32,40,256,64};cfg.useMonochromeDither=true;cfg.cachePath="/jpgpixels";bool saved=false;CHECK(jpg.decodeToFramebuffer("/jpg",r,cfg,saved));CHECK(saved);CHECK(Storage.openHandles("/jpg")==0);
  size_t black=0;for(int y=40;y<104;++y)for(int x=32;x<288;++x)black+=!(r.getWriteTarget()[y*100+x/8]&(0x80>>(x%8)));CHECK(black>7000&&black<9300);
  HalFile f;CHECK(Storage.openFileForRead("t","/jpg",f));Sink s;CHECK(JpegToBmpConverter::jpegFileTo1BitBmpStreamWithSize(f,s,64,16));++cases;
 }
 std::atomic<bool>cancel{true};ImageDecodeControl control{&cancel,0,30000};{ImageDecodeScope scope(&control);GfxRenderer r;RenderConfig cfg{0,0,64,16};CHECK(!png.decodeToFramebuffer("/png",r,cfg));CHECK(!jpg.decodeToFramebuffer("/jpg",r,cfg));}++cases;
 for(auto bytes: {std::vector<uint8_t>{'G','I','F','8','9','a',64,0,16,0},load(dir+"/mid.png")}){ImageDimsProbe probe;probe.write(bytes.data(),bytes.size());ImageDimensions d{};CHECK(probe.getDimensions(d));CHECK(d.width==64&&d.height==16);++cases;}
 CHECK(fakeheap::live.empty());
}
int main(int argc,char**argv){CHECK(argc==2);fakeheap::reset();inflateTests();zipTests();imageTests(argv[1]);std::cout<<"PASS production codec cases="<<cases<<"\n";}
