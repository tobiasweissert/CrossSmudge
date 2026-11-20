#include "FindMyCore.h"
#include <cstdio>
#include <cstdlib>
using namespace findmy;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
struct MemoryStore : Persistence {
  bool present=false, readError=false, writeError=false, uncertainWrite=false; unsigned writes=0; Checkpoint state{};
  Load load(Checkpoint& out) override { out=state; return readError?Load::Error:(present?Load::Ok:Load::Empty); }
  bool commit(const Checkpoint& next) override {
    ++writes;
    if(!writeError || uncertainWrite) { state=next; present=true; }
    return !writeError;
  }
};
int main() {
  for (unsigned b=0;b<256;++b) {
    PublicKey x{}; x[0]=static_cast<uint8_t>(b);
    for (unsigned i=1;i<28;++i) x[i]=static_cast<uint8_t>(i);
    Frame f=encodeFrame(x);
    CHECK(f.address[0]==(b|0xc0));
    CHECK(f.payload[0]==0x1e && f.payload[1]==0xff && f.payload[2]==0x4c);
    CHECK(f.payload[4]==0x12 && f.payload[5]==0x19 && f.payload[6]==0);
    CHECK(f.payload[29]==(b>>6) && f.payload[30]==0);
    CHECK(static_cast<unsigned>((f.address[0]&63)|(f.payload[29]<<6))==b);
    for (unsigned i=1;i<6;++i) CHECK(f.address[i]==x[i]);
    for (unsigned i=6;i<28;++i) CHECK(f.payload[i+1]==x[i]);
  }
  std::puts("PASS encoder: 256 first-byte round trips");
  MasterKey master{}; Seed seed{};
  for (size_t i=0;i<master.size();++i) master[i]=static_cast<uint8_t>(i+1);
  for (size_t i=0;i<seed.size();++i) seed[i]=static_cast<uint8_t>(i);
  CHECK(cryptoAvailable()); CHECK(validMaster(master));
  MasterKey invalid{}; CHECK(!validMaster(invalid));
  invalid.fill(0xff); CHECK(!validMaster(invalid));
  MasterKey order{};
  const char* orderHex="FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2E0B8F03E13DD29455C5C2A3D";
  for(size_t i=0;i<order.size();++i) {
    const char pair[]{orderHex[i*2],orderHex[i*2+1],0};
    order[i]=static_cast<uint8_t>(std::strtoul(pair,nullptr,16));
  }
  CHECK(!validMaster(order)); --order.back(); CHECK(validMaster(order));
  MasterKey one{}; one.back()=1; CHECK(validMaster(one));
  PublicKey x{}; CHECK(derivePublic(master,seed,x));
  const PublicKey expected{{0xa6,0x5c,0xc0,0xbf,0xc9,0xb5,0xfc,0x26,0x20,0x82,0x49,0xeb,0x96,0x6d,0x85,0x63,0x5a,0x3c,0x94,0xb7,0xf1,0xb6,0x0c,0x74,0x31,0xd4,0x1a,0x8a}};
  CHECK(x==expected);
  Seed next{}; CHECK(updateSeed(seed,next));
  const Seed expectedSeed{{0x09,0xa9,0xc1,0xf0,0x72,0xab,0x2a,0xa8,0x5e,0xeb,0x2f,0x10,0xb0,0x38,0xc2,0xe1,0xd2,0xa3,0x6f,0x89,0x0a,0x51,0xb3,0x39,0x42,0x68,0x80,0x8e,0x63,0x3b,0xa7,0x00}};
  CHECK(next==expectedSeed);
  CHECK(derivePublic(master,next,x));
  const PublicKey expectedNext{{0x58,0xd0,0xdd,0x8e,0xa4,0x86,0x9c,0xc3,0x68,0x16,0x41,0x95,0x03,0x37,0xc5,0xb8,0xb7,0x96,0xf7,0xec,0xfc,0x61,0xcd,0x5e,0x87,0xc4,0x71,0x7c}};
  CHECK(x==expectedNext);
  CHECK(!derivePublic(invalid,seed,x));
  std::puts("PASS crypto: independent synthetic P224/X963 index 0/1 vectors, invalid scalars");
  Provision provision{};
  CHECK(Provision::parse(master.data(),master.size(),seed.data(),seed.size(),0,1700000000,provision)==Status::Ok);
  CHECK(Provision::parse(master.data(),27,seed.data(),32,0,1700000000,provision)==Status::InvalidKey);
  CHECK(Provision::parse(master.data(),SIZE_MAX,seed.data(),32,0,1700000000,provision)==Status::InvalidKey);
  CHECK(provision.master==MasterKey{} && provision.primarySeed==Seed{});
  CHECK(Provision::parse(master.data(),28,seed.data(),33,0,1700000000,provision)==Status::InvalidKey);
  CHECK(Provision::parse(nullptr,28,seed.data(),32,0,1700000000,provision)==Status::InvalidKey);
  CHECK(Provision::parse(master.data(),28,seed.data(),31,0,1700000000,provision)==Status::InvalidKey);
  CHECK(Provision::parse(invalid.data(),28,seed.data(),32,0,1700000000,provision)==Status::InvalidKey);
  CHECK(Provision::parse(master.data(),28,seed.data(),32,0,1700000000,provision)==Status::Ok);
  MemoryStore store;
  Core core;
  CHECK(!core.provisioned()); Frame frame{}; CHECK(!core.currentFrame(frame));
  CHECK(core.init(provision,store,{1700000000,0,false},true)==Status::UntrustedTime);
  CHECK(!store.present);
  CHECK(core.init(provision,store,{1700000000,0,true},true)==Status::Ok);
  CHECK(core.provisioned() && core.currentFrame(frame));
  CHECK(store.present && store.writes==1 && store.state.index==0);
  CHECK(frame.payload==encodeFrame(expected).payload);
  CHECK(core.rotate({1700000899,899000,true})==Status::Ok);
  CHECK(store.writes==1 && core.diagnostics().index==0);
  CHECK(core.rotate({1700000900,900000,true})==Status::Ok);
  CHECK(store.writes==2 && store.state.index==1 && store.state.seed==expectedSeed);
  CHECK(core.currentFrame(frame) && frame.payload==encodeFrame(expectedNext).payload);
  std::puts("PASS core: bounded provisioning, trusted time, durable 900s rotation");
  Core reboot;
  CHECK(reboot.init(provision,store,{1700000900,0,true})==Status::ReusedIndex);
  CHECK(!reboot.currentFrame(frame) && !reboot.provisioned());
  CHECK(reboot.init(provision,store,{1700001800,0,false})==Status::UntrustedTime);
  CHECK(reboot.init(provision,store,{1700001800,0,true})==Status::Ok);
  CHECK(reboot.diagnostics().index==2 && store.state.index==2);
  const PublicKey expected2{{0x3f,0x07,0x8c,0xcb,0xc7,0x28,0xf6,0x8a,0x84,0x74,0x4c,0xb3,0xfc,0xa1,0x72,0xb2,0xc2,0x5d,0x3d,0x00,0x90,0xb4,0x3e,0x54,0xfa,0x46,0xd5,0xe2}};
  CHECK(reboot.diagnostics().publicKey==expected2);
  MemoryStore missing; Core missingCore;
  CHECK(missingCore.init(provision,missing,{1700000000,0,true})==Status::MissingState);
  MemoryStore corrupt=store; corrupt.state.version=99; Core invalidState;
  CHECK(invalidState.init(provision,corrupt,{1700002700,0,true})==Status::InvalidState);
  corrupt=store; corrupt.state.identity[0]^=1;
  CHECK(invalidState.init(provision,corrupt,{1700002700,0,true})==Status::InvalidState);
  corrupt=store; corrupt.state.lastWallSeconds=1700003600;
  CHECK(invalidState.init(provision,corrupt,{1700002700,0,true})==Status::ClockRollback);
  CHECK(invalidState.init(provision,store,{1700000900,0,true})==Status::ClockRollback);
  std::puts("PASS restart: consumed-index fence, fresh admission, checkpoint identity/version, rollback");
  CHECK(reboot.rotate({1700001799,1,true})==Status::ClockRollback);
  CHECK(!reboot.currentFrame(frame));
  CHECK(reboot.rotate({1700001800,2,true})==Status::ReusedIndex);
  CHECK(reboot.rotate({1700002700,900000,true})==Status::Ok);
  CHECK(reboot.rotate({1700002701,899999,true})==Status::ClockRollback);
  CHECK(!reboot.currentFrame(frame));
  CHECK(reboot.rotate({1700003600,1800000,true})==Status::Ok);
  CHECK(reboot.rotate({1700003600,2700000,true})==Status::UntrustedTime);
  CHECK(!reboot.currentFrame(frame));
  CHECK(reboot.rotate({1700004500,2700000,true})==Status::Ok);
  CHECK(reboot.rotate({1700004501,2701000,false})==Status::UntrustedTime);
  CHECK(!reboot.currentFrame(frame));
  CHECK(reboot.rotate({1700004501,2701000,true})==Status::ReusedIndex);
  std::puts("PASS clock: rollback, monotonic rollback, frozen wall, lost trust invalidate frame");
  struct InvalidLoad : MemoryStore { Load load(Checkpoint&) override { return static_cast<Load>(99); } } badLoad;
  Core guarded;
  CHECK(guarded.init(provision,badLoad,{1700000000,0,true},true)==Status::PersistenceError);
  MemoryStore io; io.readError=true;
  CHECK(guarded.init(provision,io,{1700000000,0,true},true)==Status::PersistenceError);
  CHECK(io.writes==0);
  io.readError=false; io.writeError=true;
  CHECK(guarded.init(provision,io,{1700000000,0,true},true)==Status::PersistenceError);
  CHECK(!guarded.currentFrame(frame) && !guarded.provisioned());
  CHECK(guarded.rotate({1700000900,900000,true})==Status::NotProvisioned);
  io.writeError=false;
  CHECK(guarded.init(provision,io,{1700000000,0,true},true)==Status::Ok);
  io.writeError=true; io.uncertainWrite=true;
  CHECK(guarded.rotate({1700000900,900000,true})==Status::PersistenceError);
  CHECK(!guarded.currentFrame(frame) && frame.payload==Frame{}.payload);
  CHECK(io.state.index==1);
  io.writeError=false;
  CHECK(guarded.init(provision,io,{1700000900,0,true})==Status::ReusedIndex);
  CHECK(guarded.init(provision,io,{1700001800,0,true})==Status::Ok);
  MemoryStore bounded; Core boundedCore;
  CHECK(boundedCore.init(provision,bounded,{1700000000+900*(Core::MaxCatchup+1),0,true},true)==Status::CatchupLimit);
  CHECK(!bounded.present && !boundedCore.currentFrame(frame));
  Provision huge=provision; huge.initialIndex=UINT64_MAX;
  CHECK(boundedCore.init(huge,bounded,{1700000900,0,true},true)==Status::IndexOverflow);
  CHECK(boundedCore.init(provision,bounded,{1699999999,0,true},true)==Status::UntrustedTime);
  std::puts("PASS persistence faults: load errors, uncertain commit, fail-closed retry, catchup/overflow bounds");
  MemoryStore damaged=io; damaged.state.seed[0]^=1;
  CHECK(guarded.init(provision,damaged,{1700002700,0,true})==Status::InvalidState);
  CHECK(!guarded.currentFrame(frame));
  damaged=io; ++damaged.state.index;
  CHECK(guarded.init(provision,damaged,{1700003600,0,true})==Status::InvalidState);
  std::puts("PASS checkpoint integrity: seed/public binding and index/wall consistency");
  MemoryStore vectorStore; Core vectorCore;
  CHECK(vectorCore.init(provision,vectorStore,{1700000000,0,true},true)==Status::Ok);
  for(uint64_t i=0;i<=96;++i) {
    if(i) CHECK(vectorCore.rotate({1700000000+i*900,i*900000,true})==Status::Ok);
    if(i==0 || i==1 || i==2 || i==96) {
      std::printf("VECTOR %llu ",static_cast<unsigned long long>(i));
      for(auto b:vectorCore.diagnostics().publicKey) std::printf("%02x",b);
      std::puts("");
    }
  }
  const Seed seed96{{0x22,0x25,0x14,0x46,0xbf,0x58,0xee,0x1e,0x8c,0x5e,0xcb,0x97,0x46,0xe0,0x65,0x7e,0x3f,0xf2,0xda,0x3f,0xfa,0x89,0xe4,0x4a,0xe5,0x3e,0x9f,0x05,0x39,0xeb,0x9f,0xc3}};
  CHECK(vectorStore.state.seed==seed96);
  CHECK(vectorStore.state.index==96 && vectorStore.writes==97);
  std::puts("PASS vectors: production ratchet through 96 steps; public-only output");
}
