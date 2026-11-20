#include "FindMyCore.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL missing crypto line %d\n",__LINE__); std::exit(1); } } while(0)
struct Store : findmy::Persistence {
  unsigned calls=0;
  findmy::Load load(findmy::Checkpoint&) override { ++calls; return findmy::Load::Empty; }
  bool commit(const findmy::Checkpoint&) override { ++calls; return true; }
};
int main() {
  using namespace findmy;
  MasterKey master{}; master.fill(1); Seed seed{}; PublicKey x{}; x.fill(0xa5);
  CHECK(!cryptoAvailable() && !validMaster(master));
  CHECK(!derivePublic(master,seed,x) && x==PublicKey{});
  Seed next{}; next.fill(0xa5); CHECK(!updateSeed(seed,next) && next==Seed{});
  Provision p;
  CHECK(Provision::parse(master.data(),28,seed.data(),32,0,1700000000,p)==Status::CryptoUnavailable);
  Store store; Core core; Frame frame;
  CHECK(core.init(p,store,{1700000000,0,true},true)==Status::CryptoUnavailable);
  CHECK(!core.currentFrame(frame) && store.calls==0);
  std::puts("PASS missing crypto: explicit unavailable, no fabricated ECC or persistence/radio activity");
}
