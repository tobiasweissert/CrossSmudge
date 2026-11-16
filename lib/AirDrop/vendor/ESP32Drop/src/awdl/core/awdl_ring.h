/* CrossSmudge adaptation: bounded single-producer/single-consumer ring cursors.
 * Payload slots remain caller-owned. Publish only after filling a slot; release
 * only after reading it. Acquire/release also orders the two ESP32-S3 cores.
 * Aligned 32-bit loads/stores compile inline without CAS or heap allocation.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

struct AwdlRing {
  uint32_t head;  /* producer advances */
  uint32_t tail;  /* consumer advances */
  uint32_t mask;  /* immutable while active; slots - 1 */
  uint32_t drops; /* producer writes; diagnostic readers use atomic loads */
};

#define AWDL_RING_POW2(n) ((n) > 0 && (((n) & ((n) - 1)) == 0))

static inline void awdl_ring_init(struct AwdlRing *r, uint32_t slots) {
  __atomic_store_n(&r->head, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&r->tail, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&r->drops, 0, __ATOMIC_RELAXED);
  r->mask = slots - 1;
}

static inline int awdl_ring_reserve(struct AwdlRing *r) {
  const uint32_t head = __atomic_load_n(&r->head, __ATOMIC_RELAXED);
  if (((head + 1) & r->mask) == __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE)) {
    const uint32_t drops = __atomic_load_n(&r->drops, __ATOMIC_RELAXED);
    __atomic_store_n(&r->drops, drops + 1, __ATOMIC_RELAXED);
    return -1;
  }
  return (int)head;
}

static inline void awdl_ring_commit(struct AwdlRing *r) {
  const uint32_t head = __atomic_load_n(&r->head, __ATOMIC_RELAXED);
  __atomic_store_n(&r->head, (head + 1) & r->mask, __ATOMIC_RELEASE);
}

static inline int awdl_ring_peek(const struct AwdlRing *r) {
  const uint32_t tail = __atomic_load_n(&r->tail, __ATOMIC_RELAXED);
  if (tail == __atomic_load_n(&r->head, __ATOMIC_ACQUIRE)) return -1;
  return (int)(tail & r->mask);
}

static inline void awdl_ring_release(struct AwdlRing *r) {
  const uint32_t tail = __atomic_load_n(&r->tail, __ATOMIC_RELAXED);
  __atomic_store_n(&r->tail, (tail + 1) & r->mask, __ATOMIC_RELEASE);
}

/* A moving diagnostic snapshot, never an admission decision. */
static inline uint32_t awdl_ring_used(const struct AwdlRing *r) {
  return (__atomic_load_n(&r->head, __ATOMIC_ACQUIRE) -
          __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE)) & r->mask;
}
