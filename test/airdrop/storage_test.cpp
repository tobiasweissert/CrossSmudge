#include <openssl/sha.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "StorageSink.h"
#include "../../lib/AirDrop/stream/StreamDecoder.h"

using namespace airdrop::storage;
struct MemoryFiles : Files {
  std::map<std::string, std::vector<uint8_t>> files;
  std::set<std::string> directories{"/"};
  std::string stage, reader;
  size_t position = 0;
  unsigned operations = 0, failAt = 0, renames = 0;
  bool stop = false, aliasFalse = false, readError = false;
  SHA256_CTX hash{};
  bool ok() { return ++operations != failAt; }
  bool ready(uint64_t& available) override { available = 100000000; return ok(); }
  PathState stat(const char* path, uint32_t& size) override {
    if (!ok() || readError) return PathState::Unreadable;
    if (directories.count(path)) return PathState::Directory;
    const auto found = files.find(path);
    if (found != files.end()) { size = found->second.size(); return PathState::File; }
    const std::string name(path); auto slash = name.rfind('/');
    return directories.count(slash == 0 ? "/" : name.substr(0, slash)) ? PathState::Missing : PathState::Unreadable;
  }
  bool mkdirExclusive(const char* path) override {
    if (!ok() || directories.count(path) || files.count(path)) return false;
    directories.insert(path); return true;
  }
  bool createStageExclusive(const char* path) override {
    if (!ok() || files.count(path)) return false;
    files[path] = {}; stage = path; return true;
  }
  size_t writeStage(const uint8_t* bytes, size_t length) override {
    assert(length <= ChunkBytes);
    if (!ok() || stage.empty()) return 0;
    files[stage].insert(files[stage].end(), bytes, bytes + length); return length;
  }
  bool syncStage() override { return ok() && !stage.empty(); }
  bool closeStage() override { const bool success = ok(); stage.clear(); return success; }
  bool writeRecordExclusive(const char* path, const uint8_t* bytes, size_t length) override {
    if (!ok() || files.count(path)) return false;
    files[path] = {bytes, bytes + length}; return true;
  }
  bool openRead(const char* path, uint32_t& size) override {
    if (!ok() || !files.count(path)) return false;
    reader = path; position = 0; size = files[path].size(); return true;
  }
  int read(uint8_t* bytes, size_t length) override {
    if (!ok() || reader.empty()) return -1;
    const auto& source = files[reader]; length = std::min(length, source.size() - position);
    std::memcpy(bytes, source.data() + position, length); position += length; return length;
  }
  bool seek(uint32_t offset) override {
    if (!ok() || reader.empty() || offset > files[reader].size()) return false;
    position = offset; return true;
  }
  bool closeRead() override { const bool success = ok(); reader.clear(); return success; }
  bool renameExclusive(const char* oldPath, const char* newPath) override {
    ++renames;
    if (!ok() || files.count(newPath) || !files.count(oldPath)) return false;
    files[newPath] = files[oldPath];
    if (aliasFalse) return false;
    files.erase(oldPath); return true;
  }
  bool hashStart() override { return ok() && SHA256_Init(&hash) == 1; }
  bool hashUpdate(const uint8_t* bytes, size_t length) override { return ok() && SHA256_Update(&hash, bytes, length) == 1; }
  bool hashFinish(uint8_t digest[32]) override { return ok() && SHA256_Final(digest, &hash) == 1; }
  bool cancelled() const override { return stop; }
};
static constexpr uint8_t token[16] = {1, 2, 3, 4, 5};
static bool write(StorageSink& sink, size_t length) {
  std::array<uint8_t, ChunkBytes> chunk{}; chunk.fill('a');
  while (length) { const size_t n = std::min(length, chunk.size()); if (!sink.append(chunk.data(), n)) return false; length -= n; }
  return true;
}
static void basic() {
  MemoryFiles files; files.files["/existing.txt"] = {'o', 'l', 'd'};
  StorageSink sink(files); assert(sink.begin("book.txt", 2049, token)); assert(write(sink, 2049));
  assert(sink.finish()); assert(sink.state() == State::PublishedVerified); assert(sink.committedBytes() == 2049);
  const std::string final = sink.savedPath(); assert(files.files[final].size() == 2049);
  assert(files.files["/existing.txt"] == std::vector<uint8_t>({'o', 'l', 'd'}));
  StorageSink recovered(files); assert(recovered.inspect("01020304050000000000000000000000") == State::PublishedVerified);
  assert(final == recovered.savedPath());
  assert(!recovered.begin("book.txt", 2049, token)); // Exclusive session names cannot be reused.
  std::puts("PASS checked publication and fresh-owner read-only recovery");
}
static void unsafeAndCancel() {
  { MemoryFiles files; files.stop = true; StorageSink sink(files); assert(!sink.begin("book.txt", 3, token)); assert(files.files.empty()); }
  for (const char* name : {"../book.txt", "CON.txt", "bad .txt"}) {
    MemoryFiles files; StorageSink sink(files); assert(!sink.begin(name, 3, token)); assert(files.files.empty());
  }
  MemoryFiles files; StorageSink sink(files); assert(sink.begin("book.txt", 3, token));
  const uint8_t binary[] = {0xc0, 0x80, 0}; assert(sink.append(binary, 3));
  files.files.at(files.stage)[0] ^= 1; // Readback must still reject changed opaque bytes.
  assert(!sink.finish()); assert(files.renames == 0);
  MemoryFiles cancelled; StorageSink stopping(cancelled); assert(stopping.begin("book.txt", 3, token));
  assert(write(stopping, 2)); stopping.cancel(); assert(stopping.state() == State::Cancelled); assert(cancelled.renames == 0);
  StorageSink incomplete(cancelled); assert(incomplete.inspect("01020304050000000000000000000000") == State::RetainedPartial);
  std::puts("PASS unsafe names, hash mismatch and cancel never publish");
}
static void uncertainty() {
  MemoryFiles files; StorageSink sink(files); assert(sink.begin("book.txt", 2049, token)); assert(write(sink, 2049));
  files.aliasFalse = true; assert(!sink.finish()); assert(sink.state() == State::Uncertain);
  const unsigned calls = files.operations; sink.cancel(); assert(files.operations == calls);
  StorageSink fresh(files); assert(fresh.inspect("01020304050000000000000000000000") == State::Uncertain);
  assert(files.renames == 1); // Recovery never retries a possibly published alias.
  size_t payloads = 0; for (const auto& entry : files.files) if (entry.second.size() == 2049) ++payloads;
  assert(payloads == 2);
  std::puts("PASS publication uncertainty preserves both possible aliases and forbids retry");
}
// Model cancellation/deadline expiry inside the final journal read or close.
// The I/O began while live; completion must recheck before exposing success.
struct FinalRecordCancellationFiles : MemoryFiles {
  bool expireOnRead = false;
  int read(uint8_t* bytes, size_t length) override {
    const bool finalRecord = reader.ends_with("/phase.4");
    const int result = MemoryFiles::read(bytes, length);
    if (finalRecord && expireOnRead) stop = true;
    return result;
  }
  bool closeRead() override {
    const bool finalRecord = reader.ends_with("/phase.4");
    const bool result = MemoryFiles::closeRead();
    if (finalRecord && !expireOnRead) stop = true;
    return result;
  }
};
static void finalRecordCancellation() {
  for (const bool duringRead : {true, false}) {
    FinalRecordCancellationFiles files;
    files.expireOnRead = duringRead;
    StorageSink sink(files);
    assert(sink.begin("book.txt", 2049, token));
    assert(write(sink, 2049));
    assert(!sink.finish());
    assert(sink.state() == State::Uncertain && sink.committedBytes() == 0);
    assert(files.stage.empty() && files.reader.empty() && files.renames == 1);
    const std::string final = "/Inbox/book (AirDrop 01020304050000000000000000000000).txt";
    assert(files.files[final].size() == 2049);
    assert(std::all_of(files.files[final].begin(), files.files[final].end(), [](uint8_t c) { return c == 'a'; }));
    const auto retained = files.files;
    const unsigned operations = files.operations;
    sink.cancel();
    assert(files.files == retained && files.operations == operations && files.renames == 1);
    // A distinct future owner may inspect the complete durable receipt; the
    // cancelled session never rolls publication back or claims sender success.
    MemoryFiles remounted = files;
    remounted.stop = false;
    StorageSink recovered(remounted);
    assert(recovered.inspect("01020304050000000000000000000000") == State::PublishedVerified);
    assert(remounted.files == retained && remounted.renames == 1);
  }
  std::puts("PASS final-record read/close cancellation refuses success, closes handles and preserves publication");
}
static void failures() {
  MemoryFiles baseline; StorageSink count(baseline); assert(count.begin("book.txt", 2049, token));
  assert(write(count, 2049)); assert(count.finish()); const unsigned operations = baseline.operations;
  unsigned tested = 0;
  for (unsigned fail = 1; fail <= operations; ++fail) {
    MemoryFiles files; files.files["/existing.txt"] = {'o', 'l', 'd'}; files.failAt = fail;
    StorageSink sink(files); const bool began = sink.begin("book.txt", 2049, token);
    const bool appended = began && write(sink, 2049); const bool completed = appended && sink.finish();
    assert(!completed); assert(sink.committedBytes() == 0);
    assert(files.files["/existing.txt"] == std::vector<uint8_t>({'o', 'l', 'd'})); ++tested;
  }
  std::printf("PASS failure at each checked adapter operation refuses false success: %u injections\n", tested);
}
static std::vector<uint8_t> odc(const char* name, uint32_t length, uint32_t mode = 0100644) {
  char header[77];
  std::snprintf(header, sizeof header, "070707%06o%06o%06o%06o%06o%06o%06o%011o%06o%011o", 0, 1, mode,
                0, 0, 1, 0, 0, unsigned(std::strlen(name) + 1), length);
  std::vector<uint8_t> result(header, header + 76);
  result.insert(result.end(), name, name + std::strlen(name) + 1);
  return result;
}
static airdrop::stream::WriteResult streamAppend(void* context, const uint8_t* bytes, size_t size) {
  return static_cast<StorageSink*>(context)->append(bytes, size) ?
    airdrop::stream::WriteResult::Consumed : airdrop::stream::WriteResult::Error;
}
static bool streamCancel(void*) { return false; }
static void large() {
  for (const bool compressed : {false, true}) {
    MemoryFiles files; StorageSink sink(files);
    assert(!sink.begin("big.txt", MaxPayloadBytes + 1, token));
    assert(sink.begin("big.txt", MaxPayloadBytes, token));
    auto decoder = std::make_unique<airdrop::stream::Decoder>();
    airdrop::stream::Manifest manifest{};
    std::strcpy(manifest.name, "big.txt"); std::strcpy(manifest.archivePath, "./big.txt"); manifest.size = MaxPayloadBytes;
    assert(decoder->reset(manifest, {&sink, streamAppend, streamCancel}));
    auto archive = odc("./big.txt", MaxPayloadBytes);
    uint32_t payload = 0;
    while (payload < MaxPayloadBytes) {
      const size_t bytes = std::min<size_t>(131072 - archive.size(), MaxPayloadBytes - payload);
      archive.insert(archive.end(), bytes, 'a'); payload += bytes;
      if (payload == MaxPayloadBytes) { const auto trailer = odc("TRAILER!!!", 0, 0); archive.insert(archive.end(), trailer.begin(), trailer.end()); }
      std::vector<uint8_t> body;
      if (compressed) {
        body.resize(compressBound(archive.size())); uLongf size = body.size();
        assert(compress2(body.data(), &size, archive.data(), archive.size(), 6) == Z_OK); body.resize(size);
      } else body = archive;
      const uint32_t word = body.size() | (compressed ? 0 : 0x80000000u);
      const uint8_t header[] = {uint8_t(word >> 24), uint8_t(word >> 16), uint8_t(word >> 8), uint8_t(word)};
      size_t used = 0; assert(decoder->feed(header, sizeof header, used) == airdrop::stream::Status::Progress && used == sizeof header);
      for (size_t at = 0; at < body.size();) {
        const size_t size = std::min<size_t>(997, body.size() - at); used = 0;
        assert(decoder->feed(body.data() + at, size, used) == airdrop::stream::Status::Progress && used == size); at += used;
      }
      archive.clear();
    }
    assert(decoder->finish() == airdrop::stream::Status::Complete); assert(sink.finish());
    const auto& stored = files.files[sink.savedPath()];
    assert(stored.size() == MaxPayloadBytes && std::all_of(stored.begin(), stored.end(), [](uint8_t c) { return c == 'a'; }));
    assert(sink.committedBytes() == MaxPayloadBytes);
    for (const uint8_t byte : files.files[sink.savedPath()]) assert(byte == 'a');
  }
  std::printf("PASS exact %u-byte stored/zlib decoder -> production sink -> verified publication; fixed sink workspace=%zu bytes\n", MaxPayloadBytes, sizeof(StorageSink));
}

static void little(std::vector<uint8_t>& out, uint32_t value, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i) out.push_back(uint8_t(value >> (8 * i)));
}
static void textBytes(std::vector<uint8_t>& out, const std::string& text) { out.insert(out.end(), text.begin(), text.end()); }
static std::vector<uint8_t> epub(const std::string& packagePath = "book.opf", const std::string& comment = "") {
  const std::pair<std::string, std::string> entries[] = {
    {"mimetype", "application/epub+zip"},
    {"META-INF/container.xml", "<container><rootfiles><rootfile full-path=\"book.opf\"/></rootfiles></container>"},
    {packagePath, "<package><metadata/><manifest/><spine/></package>"}
  };
  std::vector<uint8_t> output, central;
  for (const auto& [name, body] : entries) {
    const uint32_t offset = output.size(), crc = journalCrc32(reinterpret_cast<const uint8_t*>(body.data()), body.size());
    little(output, 0x04034b50, 4); little(output, 20, 2); little(output, 0, 2); little(output, 0, 2);
    little(output, 0, 4); little(output, crc, 4); little(output, body.size(), 4); little(output, body.size(), 4);
    little(output, name.size(), 2); little(output, 0, 2); textBytes(output, name); textBytes(output, body);
    little(central, 0x02014b50, 4); little(central, 20, 2); little(central, 20, 2); little(central, 0, 2);
    little(central, 0, 2); little(central, 0, 4); little(central, crc, 4); little(central, body.size(), 4);
    little(central, body.size(), 4); little(central, name.size(), 2); little(central, 0, 2); little(central, 0, 2);
    little(central, 0, 2); little(central, 0, 2); little(central, 0, 4); little(central, offset, 4); textBytes(central, name);
  }
  const uint32_t centralOffset = output.size(); output.insert(output.end(), central.begin(), central.end());
  little(output, 0x06054b50, 4); little(output, 0, 2); little(output, 0, 2); little(output, 3, 2); little(output, 3, 2);
  little(output, central.size(), 4); little(output, centralOffset, 4); little(output, comment.size(), 2); textBytes(output, comment);
  return output;
}
static bool storeBytes(MemoryFiles& files, StorageSink& sink, const char* name, const std::vector<uint8_t>& bytes) {
  (void)files;
  if (!sink.begin(name, bytes.size(), token)) return false;
  for (size_t at = 0; at < bytes.size();) {
    const size_t n = std::min(ChunkBytes, bytes.size() - at);
    if (!sink.append(bytes.data() + at, n)) return false;
    at += n;
  }
  return sink.finish();
}
static void contents() {
  for (const auto& bytes : {epub(), epub("book.opf", std::string("PK\5\6", 4))}) {
    MemoryFiles files; StorageSink sink(files); assert(storeBytes(files, sink, "book.epub", bytes));
    StorageSink fresh(files); assert(fresh.inspect("01020304050000000000000000000000") == State::PublishedVerified);
  }
  const std::vector<uint8_t> binary = {0x00, 0xff, 0x80, 0x01, 0x50, 0x4b};
  for (const char* name : {"report.pdf", "photo.jpg", "bundle.zip", "firmware.bin", "font.ttf", "README", "run.sh", "book.exe", "opaque.epub", "opaque.txt"}) {
    MemoryFiles files; StorageSink sink(files);
    assert(storeBytes(files, sink, name, binary));
    assert(files.files.at(sink.savedPath()) == binary);
    StorageSink fresh(files);
    assert(fresh.inspect("01020304050000000000000000000000") == State::PublishedVerified);
  }
  std::vector<uint8_t> utf8; utf8.reserve(1027); utf8.insert(utf8.end(), 1023, 'a');
  for (const uint8_t c : {0xf0, 0x9f, 0x93, 0x96}) utf8.push_back(c);
  { MemoryFiles files; StorageSink sink(files); assert(storeBytes(files, sink, "book.txt", utf8)); }
  utf8.pop_back();
  { MemoryFiles files; StorageSink sink(files); assert(storeBytes(files, sink, "book.txt", utf8)); }
  { MemoryFiles files; StorageSink sink(files); assert(storeBytes(files, sink, "empty.txt", {})); }
  std::puts("PASS opaque arbitrary files, extensionless names, binary bytes and journal recovery");
}
static void recoveryFaults() {
  MemoryFiles original; StorageSink success(original); assert(success.begin("book.txt", 3, token)); assert(write(success, 3)); assert(success.finish());
  const std::string directory = success.recoveryPath(), final = success.savedPath(), stage = directory + "/payload.part";
  { MemoryFiles moved = original; moved.files.erase(final); StorageSink receipt(moved);
    assert(receipt.inspectJournal("01020304050000000000000000000000") == State::PublishedVerified);
    assert(receipt.inspect("01020304050000000000000000000000") == State::Uncertain); }
  for (unsigned mode = 0; mode < 5; ++mode) {
    MemoryFiles files = original;
    if (mode == 0) files.files[directory + "/phase.4"].resize(511);
    if (mode == 1) files.files[directory + "/phase.2"][80] ^= 1;
    if (mode == 2) files.files.erase(directory + "/phase.3");
    if (mode == 3) files.files.erase(directory + "/phase.4");
    if (mode == 4) files.readError = true;
    const auto before = files.files; StorageSink recovered(files);
    assert(recovered.inspect("01020304050000000000000000000000") == State::Uncertain);
    assert(files.files == before && files.renames == original.renames);
  }
  MemoryFiles complete = original; complete.files[stage] = complete.files[final]; complete.files.erase(final);
  complete.files.erase(directory + "/phase.3"); complete.files.erase(directory + "/phase.4");
  StorageSink staged(complete); assert(staged.inspect("01020304050000000000000000000000") == State::CompleteStaged);
  const unsigned renames = complete.renames; assert(!staged.finish()); assert(complete.renames == renames);
  MemoryFiles collision; collision.directories.insert("/Inbox"); collision.files[final] = {'o', 'l', 'd'};
  StorageSink receiving(collision); assert(receiving.begin("book.txt", 3, token)); assert(write(receiving, 3)); assert(!receiving.finish());
  assert(collision.renames == 0 && collision.files[final] == std::vector<uint8_t>({'o', 'l', 'd'}));
  std::puts("PASS torn/gapped/unreadable journals and existing destination collisions preserve candidates without republish");
}

int main() { finalRecordCancellation(); assert(journalCrc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xcbf43926); contents(); recoveryFaults(); basic(); unsafeAndCancel(); uncertainty(); failures(); large(); return 0; }
