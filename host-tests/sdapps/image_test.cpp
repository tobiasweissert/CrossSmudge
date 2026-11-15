#include "ImageBudget.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
static void word(uint8_t* p, uint32_t v) { for (unsigned i=0;i<4;++i) p[i]=v>>(8*i); }
int main(int argc, char** argv) {
  for (int i=1;i<argc;++i) {
    std::ifstream file(argv[i], std::ios::binary);
    assert(file);
    std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)), {});
    assert(sdapps::imageWithinBudget(image.data(), image.size()));
  }
  std::array<uint8_t, 256> b{};
  memcpy(b.data(), "\177ELF\1\1\1", 7);
  b[16]=3; b[18]=94; b[20]=1; b[40]=52; b[46]=40; b[48]=3;
  word(b.data()+32, 52);
  word(b.data()+92+4, 1); word(b.data()+92+8, 6);
  word(b.data()+92+12, 0x1000); word(b.data()+92+16, 200); word(b.data()+92+20, 16);
  assert(sdapps::imageWithinBudget(b.data(), b.size()));
  for (size_t n=0;n<172;++n) assert(!sdapps::imageWithinBudget(b.data(),n));
  assert(!sdapps::imageWithinBudget(nullptr, b.size()));
  auto bad=b; bad[18]=243; assert(!sdapps::imageWithinBudget(bad.data(),bad.size()));
  bad=b; word(bad.data()+32,0xfffffff0); assert(!sdapps::imageWithinBudget(bad.data(),bad.size()));
  bad=b; word(bad.data()+92+20,0xffffffff); assert(!sdapps::imageWithinBudget(bad.data(),bad.size()));
  bad=b; bad[48]=97; assert(!sdapps::imageWithinBudget(bad.data(),bad.size()));
  bad=b; word(bad.data()+132+4,8); word(bad.data()+132+8,3);
  word(bad.data()+132+12,0x80000000); word(bad.data()+132+20,4);
  assert(!sdapps::imageWithinBudget(bad.data(),bad.size()));
  bad=b; word(bad.data()+132+4,8); word(bad.data()+132+8,3);
  word(bad.data()+132+12,0x2000); word(bad.data()+132+20,0x300000);
  assert(!sdapps::imageWithinBudget(bad.data(),bad.size()));
  bad=b; word(bad.data()+132+4,9); assert(!sdapps::imageWithinBudget(bad.data(),bad.size()));
}
