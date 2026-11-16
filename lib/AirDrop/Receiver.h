#pragma once
#include <AirDrop.h>
#include <core/Consent.h>

namespace airdrop {
enum class State : uint8_t { Stopped, Starting, Listening, Pending, Receiving, Publishing, Success, UrlReady, Stopping, Error };
struct Status {
  State state=State::Stopped;
  Offer offer;
  OfferIdentity identity;
  uint64_t deadlineMs=0,nowMs=0,wireBytes=0,payloadBytes=0,committedBytes=0,revision=0;
  char savedPath[256]{};
  bool ownershipUncertain=false,publicationUncertain=false;
};
// Boot-lifetime owner: an unexpected activity exit revokes admission but cannot
// destroy objects still used by radio/TLS tasks. No callback holds an Activity*.
class Receiver {
 public:
  static Receiver& instance();
  bool begin();
  void requestStop();
  bool stopped() const;
  void reap(); // main-task only, after stopped(): releases joined session workspaces
  void snapshot(Status& out) const;
  bool decide(const OfferIdentity& identity, bool accept);
  bool resolvePendingUrl(const OfferIdentity& identity, bool opened);
};
}  // namespace airdrop
