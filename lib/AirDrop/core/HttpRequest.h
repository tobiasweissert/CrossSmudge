#pragma once
#include <cstddef>
#include <cstdint>
namespace airdrop {
enum class Route : uint8_t { None, Discover, Ask, Upload };
enum class Framing : uint8_t { None, ContentLength, Chunked };
enum class HttpState : uint8_t { NeedMore, Ready, Error };
struct HttpRequest {
  Route route = Route::None;
  Framing framing = Framing::None;
  uint64_t length = 0;
  bool expectContinue = false;
  char transferId[37]{};
  char contentType[65]{};
};
class RequestParser {
 public:
  HttpState feed(const uint8_t* data, size_t size, size_t& consumed);
  const HttpRequest& request() const { return parsed; }
 private:
  HttpRequest parsed{};
  char line[1025]{}; // Session-owned reusable line; never allocated on the TLS stack.
  size_t used = 0;
  uint32_t bytes = 0;
  uint16_t headers = 0;
  bool first = true, cr = false, lengthSeen = false, transferSeen = false, typeSeen = false, expectSeen = false;
  HttpState state = HttpState::NeedMore;
  bool finishLine();
};
enum class SinkResult : uint8_t { Consumed, WouldBlock, Error };
using BodySink = SinkResult (*)(void*, const uint8_t*, size_t);
enum class BodyState : uint8_t { NeedMore, Complete, WouldBlock, Error };
// Constant storage, independent of HTTP chunk/body size. Sink calls are <=2048 bytes.
// Caller owns absolute transport deadline and checks stop between feed/read calls.
class BodyDecoder {
 public:
  explicit BodyDecoder(const HttpRequest& request, uint64_t maxBody = 70000000, uint64_t maxWire = 72000000);
  BodyState feed(const uint8_t* data, size_t size, size_t& consumed, BodySink sink, void* context);
  bool complete() const { return phase == Phase::Done; }
  uint64_t payloadBytes() const { return payload; }
  uint64_t wireBytes() const { return wire; }
 private:
  enum class Phase : uint8_t { Size, SizeLf, Data, DataCr, DataLf, EndCr, EndLf, Done, Failed };
  Phase phase = Phase::Failed;
  Framing framing;
  uint64_t remaining = 0, chunk = 0, payload = 0, wire = 0, bodyLimit, wireLimit;
  uint32_t chunks = 0;
  uint8_t digits = 0;
};
}  // namespace airdrop
