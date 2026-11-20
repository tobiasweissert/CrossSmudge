#pragma once

namespace crosscub::findmy {
// Admission only: this never equates a configured identity with a live radio,
// and never changes legacy sleep when the owner has not explicitly opted in.
class TrackingPolicy {
 public:
  void update(bool optedIn, bool provisioned, bool trustedTime, bool advertising) {
    optedIn_ = optedIn;
    provisioned_ = provisioned;
    trustedTime_ = trustedTime;
    advertising_ = advertising;
  }
  bool shouldReplaceDeepSleep() const {
    return optedIn_ && provisioned_ && trustedTime_ && advertising_;
  }
 private:
  bool optedIn_ = false;
  bool provisioned_ = false;
  bool trustedTime_ = false;
  bool advertising_ = false;
};
}  // namespace crosscub::findmy
