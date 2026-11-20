#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace findmy {
using PublicKey = std::array<uint8_t,28>;
using MasterKey = std::array<uint8_t,28>;
using Seed = std::array<uint8_t,32>;
using Random = int (*)(void*, unsigned char*, size_t);
bool cryptoAvailable();
bool validMaster(const MasterKey&);
bool updateSeed(const Seed&, Seed&);
bool derivePublic(const MasterKey&, const Seed&, PublicKey&, Random = nullptr, void* = nullptr);
void secureErase(void*, size_t);
struct Frame {
  // Human/controller display order, NOT NimBLE's reversed internal byte order.
  std::array<uint8_t,6> address{};
  std::array<uint8_t,31> payload{};
};
inline Frame encodeFrame(const PublicKey& x) {
  Frame f; f.payload={{0x1e,0xff,0x4c,0,0x12,0x19,0}};
  for (size_t i=0;i<6;++i) f.address[i]=x[i];
  f.address[0]|=0xc0;
  for (size_t i=6;i<28;++i) f.payload[i+1]=x[i];
  f.payload[29]=x[0]>>6;
  return f;
}
enum class Status : uint8_t {
  Ok, InvalidKey, CryptoUnavailable, CryptoFailure, UntrustedTime, ClockRollback,
  MissingState, PersistenceError, InvalidState, ReusedIndex, CatchupLimit, IndexOverflow, NotProvisioned
};
struct Provision {
  MasterKey master{};
  Seed primarySeed{};
  uint64_t initialIndex=0;
  uint64_t epochSeconds=0; // Unix time at start of initialIndex, not necessarily pairing time.
  ~Provision() { secureErase(master.data(),master.size()); secureErase(primarySeed.data(),primarySeed.size()); }
  static Status parse(const uint8_t* master, size_t masterSize, const uint8_t* seed,
                      size_t seedSize, uint64_t initialIndex, uint64_t epochSeconds, Provision& out);
};
struct Time {
  uint64_t unixSeconds;
  uint64_t monotonicMs; // caller extends platform millis() wrap; never persistent across reboot.
  bool trusted;
};
struct Checkpoint {
  uint32_t version=1;
  PublicKey identity{}; // public x at provisioned initial index
  PublicKey publicKey{}; // public x bound to current checkpoint seed
  Seed seed{}; // SECRET: current ratchet seed, never diagnostics
  uint64_t initialIndex=0, epochSeconds=0, index=0, lastWallSeconds=0;
  ~Checkpoint() { secureErase(seed.data(),seed.size()); }
};
enum class Load : uint8_t { Ok, Empty, Error };
struct Persistence {
  virtual ~Persistence() = default;
  virtual Load load(Checkpoint&) = 0;
  // Must be durable + atomic and read-back verified BEFORE returning true.
  // False can mean uncertain write: core latches unavailable until re-init.
  virtual bool commit(const Checkpoint&) = 0;
};
struct Diagnostics {
  Status status;
  bool provisioned, ready;
  uint64_t index;
  PublicKey publicKey;
};
class Core {
 public:
  explicit Core(Random rng=nullptr, void* rngContext=nullptr) : rng_(rng), rngContext_(rngContext) {}
  ~Core();
  Core(const Core&)=delete;
  Core& operator=(const Core&)=delete;
  Status init(const Provision&, Persistence&, const Time&, bool allowFresh=false);
  Status rotate(const Time&);
  bool provisioned() const { return provisioned_; }
  bool currentFrame(Frame&) const;
  Diagnostics diagnostics() const { return {status_,provisioned_,ready_,state_.index,public_}; }
  static constexpr uint64_t RotationSeconds=900, MaxCatchup=4096;
 private:
  Status fail(Status s);
  Status publish(Checkpoint&, const Time&);
  Provision provision_{};
  Checkpoint state_{};
  Persistence* persistence_=nullptr;
  Random rng_;
  void* rngContext_;
  Time lastTime_{0,0,false}, anchorTime_{0,0,false};
  PublicKey public_{};
  Frame frame_{};
  Status status_=Status::NotProvisioned;
  bool provisioned_=false, ready_=false;
};
}
