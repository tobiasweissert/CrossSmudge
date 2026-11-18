#pragma once
#include "Offer.h"
namespace airdrop {
struct OfferIdentity {
  uint64_t generation = 0, session = 0, offer = 0, connection = 0;
  uint32_t interfaceIndex = 0;
  uint8_t peer[16]{};
};
enum class DecisionState : uint8_t { Idle, Pending, Accepted, Declined, Expired, Cancelled, Uploading, Consumed };
// Single protocol-owner state machine. Other tasks must enqueue owned decisions;
// caller serializes all methods. No renderer/Lua/storage callbacks occur here.
class Consent {
 public:
  bool begin(const Offer& offer, const OfferIdentity& identity, uint64_t askStartedMs, uint64_t nowMs);
  bool decide(const OfferIdentity& identity, bool accept, uint64_t nowMs);
  void tick(uint64_t nowMs);
  void cancel();
  void disconnect(uint64_t connection);
  bool authorizeUpload(const OfferIdentity& peer, const char* transferId, uint64_t nowMs,
                       bool admitValidatedReconnect = false);
  bool consumeUrl(const OfferIdentity& identity, uint64_t nowMs);
  void completeUpload();
  DecisionState state() const { return current; }
  uint64_t deadlineMs() const { return deadline; }
  const Offer& offer() const { return incoming; }
  const OfferIdentity& identity() const { return owner; }

 private:
  Offer incoming{};
  OfferIdentity owner{};
  uint64_t deadline = 0;
  DecisionState current = DecisionState::Idle;
  bool sameToken(const OfferIdentity& identity) const;
};
}  // namespace airdrop
