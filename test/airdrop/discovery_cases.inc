struct Golden {
  const char *name,*instance;uint8_t master[6];uint32_t tsf_us;int have_window;
  uint32_t rx_now_us,phase_us,metric,master_counter;uint16_t cur_aw_seq;const char* hex;
};
static const Golden goldens[]={
#include "discovery_mif_golden.inc"
};
static std::vector<uint8_t> decodeHex(const char* hex){std::vector<uint8_t> out;while(*hex){unsigned byte=0;assert(std::sscanf(hex,"%2x",&byte)==1);out.push_back(uint8_t(byte));hex+=2;}return out;}
static void save(const std::string& path,const uint8_t* data,size_t n){std::ofstream f(path,std::ios::binary);assert(f);f.write(reinterpret_cast<const char*>(data),n);assert(f);}
int main(int argc,char** argv){
  assert(argc==2);std::string root=argv[1];int failures=0;
  using namespace airdrop::discovery;
  assert(start()&&tap);std::ifstream cases(root+"/cases.tsv");assert(cases);std::string key;int expected;
  while(cases>>key>>expected){std::ifstream f(root+"/"+key+".ip6",std::ios::binary);assert(f);std::vector<uint8_t> packet{std::istreambuf_iterator<char>(f),{}};sends=0;allocations=0;
    const uint8_t peer[6]={1,2,3,4,5,6};insideCallback=true;tap(nullptr,peer,packet.data(),packet.size());insideCallback=false;
    if(sends!=unsigned(expected)){std::fprintf(stderr,"FAIL %s expectedEmit=%d actual=%u\n",key.c_str(),expected,sends);++failures;}
    if(allocations){std::fprintf(stderr,"FAIL callback allocated %zu for %s\n",allocations,key.c_str());++failures;}
    if(key=="service-ptr"&&sends)save(root+"/response.bin",sent,sentLength);
  }
  assert(detach()&&!tap);
  uint8_t buffer[AWDL_MIF_MAX];
  for(size_t i=0;i<sizeof(goldens)/sizeof(goldens[0]);++i){auto& g=goldens[i];AwdlMifParams p{};p.src=fixtureIdentity.mac;p.master=g.master;p.instance=g.instance;p.tsf_us=g.tsf_us;p.metric=g.metric;p.master_counter=g.master_counter;p.have_window=g.have_window;p.rx_now_us=g.rx_now_us;p.phase_us=g.phase_us;p.cur_aw_seq=g.cur_aw_seq;
    auto expectedBytes=decodeHex(g.hex);
#if defined(FREEINK_DEVICE_STICKY)
    static const uint8_t from[]="flags=1019",to[]="flags=0131";auto found=std::search(expectedBytes.begin(),expectedBytes.end(),from,from+10);assert(found!=expectedBytes.end());std::copy(to,to+10,found);
#endif
    size_t n=awdl_build_mif(buffer,sizeof buffer,&p);assert(n==expectedBytes.size()&&std::memcmp(buffer,expectedBytes.data(),n)==0);assert(awdl_build_mif(buffer,AWDL_MIF_MAX-1,&p)==0);
    if(!i)save(root+"/mif.bin",buffer,n);
  }
  std::printf("Discovery preserved-contract + malformed-input tests failures=%d; host extraction only\n",failures);return failures?1:0;
}
