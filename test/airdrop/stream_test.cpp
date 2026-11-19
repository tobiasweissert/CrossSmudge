#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "StreamDecoder.h"

using namespace airdrop::stream;

#define CHECK(x)                                                        \
  do {                                                                  \
    if (!(x)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
      return false;                                                     \
    }                                                                   \
  } while (0)

struct Output {
  uint64_t hash = 1469598103934665603ull;
  uint32_t bytes = 0;
  unsigned calls = 0;
  bool blocked = false;
  bool cancel = false;
  bool fail = false;
};

static WriteResult append(void* context, const uint8_t* bytes, size_t length) {
  auto& output = *static_cast<Output*>(context);
  ++output.calls;
  if (length > 1024) std::abort();
  if (output.fail) return WriteResult::Error;
  if (output.blocked && output.calls % 7 == 1) return WriteResult::WouldBlock;
  for (size_t i = 0; i < length; ++i) output.hash = (output.hash ^ bytes[i]) * 1099511628211ull;
  output.bytes += length;
  return WriteResult::Consumed;
}

static bool cancelled(void* context) { return static_cast<Output*>(context)->cancel; }

static Manifest manifest(const char* name = "book.txt", uint32_t size = 3) {
  Manifest result{};
  std::snprintf(result.name, sizeof result.name, "%s", name);
  std::snprintf(result.archivePath, sizeof result.archivePath, "%s", name);
  result.size = size;
  return result;
}

static std::vector<uint8_t> header(const std::string& name, uint32_t size, uint32_t mode = 0100644,
                                   uint32_t links = 1) {
  char bytes[77];
  std::snprintf(bytes, sizeof bytes, "070707%06o%06o%06o%06o%06o%06o%06o%011o%06o%011o", 0, 1, mode, 0, 0, links, 0, 0,
                unsigned(name.size() + 1), size);
  std::vector<uint8_t> result(bytes, bytes + 76);
  result.insert(result.end(), name.begin(), name.end());
  result.push_back(0);
  return result;
}

static void add(std::vector<uint8_t>& destination, const std::vector<uint8_t>& source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

static std::vector<uint8_t> archive(const std::string& name = "book.txt", const std::string& data = "abc",
                                    uint32_t mode = 0100644, uint32_t links = 1) {
  auto result = header(name, data.size(), mode, links);
  result.insert(result.end(), data.begin(), data.end());
  add(result, header("TRAILER!!!", 0, 0));
  return result;
}

// Host zlib creates fixture records. Production decoding links the repository's
// exact miniz_impl.c; host zlib is never used by StreamDecoder.cpp.
static std::vector<uint8_t> record(const std::vector<uint8_t>& input, bool compressed) {
  std::vector<uint8_t> body;
  if (compressed) {
    body.resize(compressBound(input.size()));
    uLongf length = body.size();
    if (compress2(body.data(), &length, input.data(), input.size(), 6) != Z_OK) std::abort();
    body.resize(length);
  } else {
    body = input;
  }
  const uint32_t word = body.size() | (compressed ? 0 : 0x80000000u);
  std::vector<uint8_t> result{uint8_t(word >> 24), uint8_t(word >> 16), uint8_t(word >> 8), uint8_t(word)};
  add(result, body);
  return result;
}

static bool push(Decoder& decoder, const uint8_t* bytes, size_t length, size_t split = 17) {
  size_t position = 0;
  unsigned iterations = 0;
  while (position < length) {
    size_t consumed = 0;
    const auto status = decoder.feed(bytes + position, std::min(split, length - position), consumed);
    position += consumed;
    if (status == Status::Error || (consumed == 0 && status != Status::WouldBlock)) return false;
    if (++iterations > length * 4 + 100) return false;
  }
  // Drain decompressed output retained after consuming the final encoded byte.
  for (unsigned i = 0; i < 10000; ++i) {
    size_t consumed = 0;
    const auto status = decoder.feed(nullptr, 0, consumed);
    if (status == Status::Error) return false;
    if (status != Status::WouldBlock) return true;
  }
  return false;
}

static bool push(Decoder& decoder, const std::vector<uint8_t>& bytes, size_t split = 17) {
  return push(decoder, bytes.data(), bytes.size(), split);
}

static bool rejected(const std::vector<uint8_t>& bytes, Manifest accepted = manifest()) {
  auto decoder = std::make_unique<Decoder>();
  Output output{};
  if (!decoder->reset(accepted, {&output, append, cancelled})) return true;
  return !push(*decoder, bytes) || decoder->finish() != Status::Complete;
}

static bool small() {
  for (const bool compressed : {false, true}) {
    for (const size_t split : {size_t(1), size_t(2), size_t(7), size_t(4096)}) {
      auto decoder = std::make_unique<Decoder>();
      Output output{};
      output.blocked = true;
      CHECK(decoder->reset(manifest(), {&output, append, cancelled}));
      const auto body = record(archive(), compressed);
      CHECK(push(*decoder, body, split));
      CHECK(decoder->finish() == Status::Complete);
      CHECK(output.bytes == 3);
      CHECK(decoder->payloadBytes() == 3);
      CHECK(decoder->encodedBytes() == body.size());
      CHECK(decoder->finish() == Status::Complete);
    }
  }
  return true;
}

static bool hostile() {
  CHECK(rejected(record(archive("../book.txt"), false)));
  CHECK(rejected(record(archive("dir/book.txt"), true)));
  CHECK(rejected(record(archive("book.txt", "abc", 0120644), false)));
  CHECK(rejected(record(archive("book.txt", "abc", 0100644, 2), false)));
  CHECK(rejected(record(archive("book.txt", "abc", 0500644), false)));
  CHECK(rejected(record(archive("other.txt"), false)));
  auto bytes = archive();
  bytes.resize(bytes.size() - 1);
  CHECK(rejected(record(bytes, false)));
  bytes = archive();
  bytes.push_back(1);
  CHECK(rejected(record(bytes, true)));
  bytes = header("book.txt", 3);
  bytes.insert(bytes.end(), {'a', 'b', 'c'});
  CHECK(rejected(record(bytes, false)));
  add(bytes, archive());
  CHECK(rejected(record(bytes, false)));
  bytes = record(archive(), true);
  bytes.back() ^= 1;
  CHECK(rejected(bytes));
  bytes = record(archive(), true);
  bytes.push_back(99);
  bytes[3]++;
  CHECK(rejected(bytes));
  bytes = record(archive(), false);
  bytes.pop_back();
  CHECK(rejected(bytes));
  CHECK(rejected({0x80, 0, 0, 0}));
  CHECK(rejected({0x80, 0x10, 0, 1}));
  CHECK(rejected(record(archive(), false), manifest("book.txt", kMaxPayload + 1)));
  CHECK(rejected(record(header("book.txt", kMaxPayload + 1), false), manifest("book.txt", kMaxPayload)));
  for (const auto name : {"foo .txt", "aux.txt", "COM1.txt", "book.txt.", "bad\\name.txt"}) {
    CHECK(rejected(record(archive(name), false), manifest(name)));
  }
  auto decoder = std::make_unique<Decoder>();
  Output output{};
  CHECK(decoder->reset(manifest(), {&output, append, cancelled}));
  output.cancel = true;
  size_t consumed = 0;
  bytes = record(archive(), false);
  CHECK(decoder->feed(bytes.data(), bytes.size(), consumed) == Status::Error);
  CHECK(output.bytes == 0);
  output = {};
  output.fail = true;
  CHECK(decoder->reset(manifest(), {&output, append, cancelled}));
  CHECK(!push(*decoder, bytes));
  CHECK(decoder->finish() == Status::Error);
  for (const char* name :
       {"report.pdf", "photo.jpg", "bundle.zip", "firmware.bin", "font.ttf", "README", "run.sh", "book.exe"}) {
    for (bool compressed : {false, true}) {
      auto accepted = std::make_unique<Decoder>();
      Output bytes{};
      CHECK(accepted->reset(manifest(name), {&bytes, append, cancelled}));
      CHECK(push(*accepted, record(archive(name, std::string("\0\xff\x80", 3), 0100755), compressed), 1));
      CHECK(accepted->finish() == Status::Complete && bytes.bytes == 3);
    }
  }
  return true;
}

static bool boundaries() {
  auto decoder = std::make_unique<Decoder>();
  Output output{};
  // Exactly 1 MiB expanded record succeeds even at a high compression ratio;
  // one more expanded byte fails by absolute quota, without a ratio heuristic.
  const uint32_t overhead = archive().size() - 3;
  const uint32_t recordPayload = kMaxRecord - overhead;
  auto bytes = archive("book.txt", std::string(recordPayload, 'x'));
  CHECK(bytes.size() == kMaxRecord);
  for (const bool compressed : {false, true}) {
    output = {};
    CHECK(decoder->reset(manifest("book.txt", recordPayload), {&output, append, cancelled}));
    CHECK(push(*decoder, record(bytes, compressed), compressed ? 1 : 997));
    CHECK(decoder->finish() == Status::Complete);
  }
  bytes = archive("book.txt", std::string(recordPayload + 1, 'x'));
  CHECK(rejected(record(bytes, true), manifest("book.txt", recordPayload + 1)));
  CHECK(rejected(record(bytes, false), manifest("book.txt", recordPayload + 1)));
  bytes = record(archive(), true);
  bytes.insert(bytes.end(), 4, 0);
  output = {};
  CHECK(decoder->reset(manifest(), {&output, append, cancelled}));
  CHECK(push(*decoder, bytes, 1));
  CHECK(decoder->finish() == Status::Complete);
  bytes.push_back(0);
  CHECK(rejected(bytes));
  bytes = record(archive(), false);
  for (size_t cut = 0; cut < bytes.size(); ++cut) {
    const std::vector<uint8_t> shortBody(bytes.begin(), bytes.begin() + cut);
    CHECK(rejected(shortBody));
  }
  // Overflowing 11-digit odc sizes and non-octal fields fail before payload.
  bytes = archive();
  std::memset(bytes.data() + 65, '7', 11);
  CHECK(rejected(record(bytes, false)));
  bytes = archive();
  bytes[6] = '8';
  CHECK(rejected(record(bytes, false)));
  bytes = archive();
  bytes[76 + 8] = 'x';
  CHECK(rejected(record(bytes, false)));
  bytes = archive();
  bytes.insert(bytes.end(), 511, 0);
  output = {};
  CHECK(decoder->reset(manifest(), {&output, append, cancelled}));
  CHECK(push(*decoder, record(bytes, false)));
  CHECK(decoder->finish() == Status::Complete);
  bytes.push_back(0);
  CHECK(rejected(record(bytes, false)));
  for (const char* name :
       {"x\xe2\x80\x8e.txt", "x\xe2\x80\x8f.txt", "x\xe2\x80\xa8.txt", "x\xe2\x80\xa9.txt", "x\xef\xbb\xbf.txt"})
    CHECK(!safeName(name, std::strlen(name)));
  CHECK(!safeName("bad\xE2\x80\xAE.txt", 10));
  CHECK(!safeName("bad\xC0\xAF.txt", 9));
  return true;
}

static bool wrappers() {
  for (const uint32_t links : {0u, 1u, 2u, 3u, 17u, 65535u})
    for (bool compressed : {false, true}) {
      auto bytes = header(".", 0, 0040755, links);
      add(bytes, archive("./book.txt"));
      auto decoder = std::make_unique<Decoder>();
      Output output{};
      auto accepted = manifest();
      std::strcpy(accepted.archivePath, "./book.txt");
      CHECK(decoder->reset(accepted, {&output, append, cancelled}));
      CHECK(push(*decoder, record(bytes, compressed), 1));
      CHECK(decoder->finish() == Status::Complete && output.bytes == 3);
    }
  auto bytes = header(".", 0, 0040755, 2);
  add(bytes, header("./._book.txt", 4));
  bytes.insert(bytes.end(), 4, 0x42);
  add(bytes, archive("./book.txt"));
  auto decoder = std::make_unique<Decoder>();
  Output output{};
  auto accepted = manifest();
  std::strcpy(accepted.archivePath, "./book.txt");
  CHECK(decoder->reset(accepted, {&output, append, cancelled}));
  CHECK(push(*decoder, record(bytes, true), 1));
  CHECK(decoder->finish() == Status::Complete);
  CHECK(output.bytes == 3);
  CHECK(rejected(record(archive(), false), accepted));
  return true;
}

static bool large(bool compressed) {
  auto decoder = std::make_unique<Decoder>();
  Output output{};
  output.blocked = true;
  CHECK(decoder->reset(manifest("big.txt", kMaxPayload), {&output, append, cancelled}));
  uint64_t expected = 1469598103934665603ull;
  uint32_t position = 0;
  auto chunk = header("big.txt", kMaxPayload);
  while (position < kMaxPayload) {
    const size_t length = std::min(size_t(131072 - chunk.size()), size_t(kMaxPayload - position));
    for (size_t i = 0; i < length; ++i) {
      const auto byte = uint8_t((position + i) * 17u + (position + i) / 251u);
      chunk.push_back(byte);
      expected = (expected ^ byte) * 1099511628211ull;
    }
    position += length;
    if (position == kMaxPayload) add(chunk, header("TRAILER!!!", 0, 0));
    CHECK(push(*decoder, record(chunk, compressed), 997));
    chunk.clear();
  }
  CHECK(decoder->finish() == Status::Complete);
  CHECK(output.bytes == kMaxPayload);
  CHECK(output.hash == expected);
  CHECK(decoder->payloadBytes() == kMaxPayload);
  CHECK(sizeof(Decoder) < 50000);
  return true;
}

static bool nativeArchive(const char* path) {
  std::ifstream file(path, std::ios::binary);
  CHECK(file);
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
  for (bool compressed : {false, true}) {
    auto decoder = std::make_unique<Decoder>();
    Output output{};
    auto accepted = manifest();
    std::strcpy(accepted.archivePath, "./book.txt");
    CHECK(decoder->reset(accepted, {&output, append, cancelled}));
    CHECK(push(*decoder, record(bytes, compressed), 1));
    CHECK(decoder->finish() == Status::Complete && output.bytes == 3);
  }
  return true;
}

int main(int argc, char** argv) {
  if (argc == 2 && !nativeArchive(argv[1])) return 1;
  if (!small() || !hostile() || !boundaries() || !wrappers() || !large(false) || !large(true)) return 1;
  std::printf("stream tests passed; decoder fixed workspace=%zu bytes; exact payload=%u bytes stored/zlib\n",
              sizeof(Decoder), kMaxPayload);
  return 0;
}
