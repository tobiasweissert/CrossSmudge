#include <awdl_ring.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <thread>

int main() {
  AwdlRing ring{};
  awdl_ring_init(&ring, 8);
  std::array<std::array<uint32_t, 16>, 8> slots{};
  constexpr uint32_t Count = 200000;
  std::thread producer([&] {
    for (uint32_t sequence = 1; sequence <= Count; ++sequence) {
      int slot;
      while ((slot = awdl_ring_reserve(&ring)) < 0) std::this_thread::yield();
      for (auto& word : slots[slot]) word = sequence;
      awdl_ring_commit(&ring);
    }
  });
  for (uint32_t sequence = 1; sequence <= Count; ++sequence) {
    int slot;
    while ((slot = awdl_ring_peek(&ring)) < 0) std::this_thread::yield();
    for (const auto word : slots[slot]) assert(word == sequence);
    awdl_ring_release(&ring);
  }
  producer.join();
  assert(awdl_ring_peek(&ring) < 0 && awdl_ring_used(&ring) == 0);
}
