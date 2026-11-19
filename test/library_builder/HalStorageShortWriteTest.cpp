#include <cassert>

#include "HalStorage.h"

int main() {
  const uint8_t bytes[] = {0x41, 0x42};
  HalFile file;
  file.node = std::make_shared<fake::Node>();
  file.path = "/short-write-probe";
  file.pos = 8;
  fake::shortWrite = 0;
  assert(file.write(bytes, sizeof(bytes)) == 0);
  assert(file.node->bytes.empty());
  assert(file.pos == 8);
  assert(fake::shortWrite == -1);
  file.pos = 0;
  fake::shortWrite = 0;
  assert(file.write(bytes, sizeof(bytes)) == 0);
  assert(file.node->bytes.empty());
  fake::shortWrite = 1;
  assert(file.write(bytes, sizeof(bytes)) == 1);
  assert(file.node->bytes == std::vector<uint8_t>({0x41}));
  assert(file.pos == 1);
  assert(file.write(bytes + 1, 1) == 1);
  assert(file.node->bytes == std::vector<uint8_t>({0x41, 0x42}));
}
