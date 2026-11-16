#include "Consent.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace airdrop {
bool Consent::sameToken(const OfferIdentity& p) const {
  return owner.generation == p.generation && owner.session == p.session && owner.offer == p.offer &&
         owner.interfaceIndex == p.interfaceIndex && std::memcmp(owner.peer, p.peer, sizeof owner.peer) == 0;
}

bool Consent::begin(const Offer& offer, const OfferIdentity& identity, uint64_t started, uint64_t now) {
  if (current == DecisionState::Pending || current == DecisionState::Accepted || current == DecisionState::Uploading) {
    return false;
  }
  // Generation/session/offer numbers are local monotonic tokens, never sender IDs.
  // Terminal decisions retain their identity so delayed UI events cannot decide a new offer.
  if (current != DecisionState::Idle &&
      (identity.generation < owner.generation ||
       (identity.generation == owner.generation &&
        (identity.session < owner.session || (identity.session == owner.session && identity.offer <= owner.offer))))) {
    return false;
  }
  if (offer.kind == OfferKind::None || !identity.generation || !identity.session || !identity.offer ||
      !identity.connection || !identity.interfaceIndex || started > now ||
      started > std::numeric_limits<uint64_t>::max() - 40000 || now > std::numeric_limits<uint64_t>::max() - 30000 ||
      now >= started + 40000) {
    return false;
  }
  incoming = offer;
  owner = identity;
  deadline = std::min(started + 40000, now + 30000);
  current = DecisionState::Pending;
  return true;
}

void Consent::tick(uint64_t now) {
  if ((current == DecisionState::Pending || current == DecisionState::Accepted) && now >= deadline) {
    current = DecisionState::Expired;
  }
}

bool Consent::decide(const OfferIdentity& token, bool accept, uint64_t now) {
  tick(now);
  if (current != DecisionState::Pending || !sameToken(token) || owner.connection != token.connection) return false;
  current = accept ? DecisionState::Accepted : DecisionState::Declined;
  return true;
}

void Consent::cancel() {
  if (current == DecisionState::Pending || current == DecisionState::Accepted || current == DecisionState::Uploading) {
    current = DecisionState::Cancelled;
  }
}

void Consent::disconnect(uint64_t connection) {
  if (owner.connection == connection) cancel();
}

bool Consent::authorizeUpload(const OfferIdentity& peer, const char* transferId, uint64_t now, bool reconnect) {
  tick(now);
  if (current != DecisionState::Accepted || incoming.kind != OfferKind::File || !sameToken(peer) || !peer.connection) {
    return false;
  }
  const bool sameConnection = owner.connection == peer.connection;
  const bool matching = transferId && std::strcmp(transferId, incoming.transferId) == 0;
  if (incoming.transferId[0] && !matching) return false;
  if (!sameConnection && (!reconnect || !incoming.transferId[0] || !matching)) return false;
  // ID-less legacy transfers require their still-live consenting TLS connection.
  if (!incoming.transferId[0] && transferId && transferId[0]) return false;
  current = DecisionState::Uploading;
  owner.connection = peer.connection;
  return true;
}

bool Consent::consumeUrl(const OfferIdentity& token, uint64_t now) {
  tick(now);
  if (current != DecisionState::Accepted || incoming.kind != OfferKind::Url || !sameToken(token) ||
      owner.connection != token.connection) {
    return false;
  }
  current = DecisionState::Consumed;
  return true;
}

void Consent::completeUpload() {
  if (current == DecisionState::Uploading) current = DecisionState::Consumed;
}
}  // namespace airdrop
