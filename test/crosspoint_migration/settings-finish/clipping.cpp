#include <ClippingStore.h>
#include <HalStorage.h>
#include <Serialization.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using Result = ClippingStore::AddResult;
Result add(ClippingStore& s, const std::string& text = "hello") {
  return s.addClipping(0, 0, 0, 1, 0, 4, 5, "chapter", 0, text, uint32_t(123), uint32_t(10), uint32_t(20));
}
std::string path() {
  for (const auto& [p, n] : fake::files)
    if (p.ends_with(".bin")) return p;
  return {};
}
void reset() {
  fake::reset();
  fake::failSync = -1;
  fake::corruptWrite = -1;
  fake::aliasRenameFailure = false;
}
void legacy(const std::string& p, uint8_t version, bool modern = false) {
  auto f = Storage.open(p.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
  serialization::tryWritePod(f, version);
  serialization::tryWritePod(f, uint16_t(1));
  for (auto s : {"title", "author", "/book.epub"}) serialization::tryWriteString(f, s);
  for (uint16_t v : {0, 0, 0, 1, 0, 4, 5, 0}) serialization::tryWritePod(f, v);
  serialization::tryWritePod(f, uint32_t(42));
  if (version >= 3) serialization::tryWritePod(f, uint32_t(123));
  if (version == 4 && !modern) serialization::tryWritePod(f, uint16_t(7));
  if (modern) {
    serialization::tryWritePod(f, uint32_t(10));
    serialization::tryWritePod(f, uint32_t(20));
    serialization::tryWritePod(f, uint64_t(44));
    serialization::tryWritePod(f, uint8_t(0));
    char id[65] = {};
    strcpy(id, "abcdef123456");
    f.write(id, sizeof(id));
  }
  char title[48] = {};
  strcpy(title, "chapter");
  f.write(title, sizeof(title));
  if (version == 1)
    serialization::tryWritePod(f, uint32_t(5));
  else
    serialization::tryWritePod(f, uint16_t(5));
  f.write("hello", 5);
  assert(f.close());
}
int main() {
  reset();
  ClippingStore s;
  assert(s.loadForBook("/book.epub", "title", "author", "epub"));
  assert(add(s) == Result::Added);
  auto p = path();
  s.unload();
  for (uint8_t v = 1; v <= 4; ++v) {
    legacy(p, v);
    const auto old = fake::files.at(p)->bytes;
    assert(s.loadForBook("/book.epub", "title", "author", "epub"));
    assert(s.clippingCount() == 1);
    assert(fake::files.at(p)->bytes == old);
    std::string text;
    assert(s.readClippingText(0, text) && text == "hello");
    if (v == 4) assert(s.clippingAt(0)->tableSelection == 7);
    assert(s.prepareSync());
    assert(fake::files.at(p)->bytes[0] == 5);
    const std::string id = s.clippingAt(0)->id;
    s.unload();
    assert(s.loadForBook("/book.epub", "title", "author", "epub"));
    assert(id == s.clippingAt(0)->id);
    assert(s.finishUploads());
    assert(!s.clippingAt(0)->pendingUpload);
    s.unload();
  }
  legacy(p, 4, true);
  assert(s.loadForBook("/book.epub", "title", "author", "epub"));
  assert(s.clippingAt(0)->syncRevision == 44 && s.clippingAt(0)->startOffset == 10);
  assert(std::string(s.clippingAt(0)->id) == "abcdef123456");
  auto remote = *s.clippingAt(0);
  remote.syncRevision = 45;
  remote.startOffset = 30;
  remote.endOffset = 40;
  assert(s.applyRemote(remote, "remote", false));
  assert(s.readClippingText(0, p) && p == "remote");
  assert(s.removeClippingAt(0));
  uint32_t offset = 0;
  char id[65];
  assert(s.nextDeletion(offset, id));
  assert(std::string(id) == remote.id);
  assert(s.applyRemote(remote, "stale", false));
  assert(!s.hasClippings());
  s.unload();
  assert(s.loadForBook("/book.epub", "title", "author", "epub"));
  offset = 0;
  assert(s.nextDeletion(offset, id) && id[0]);
  assert(s.finishDeletions());
  s.unload();
  assert(s.loadForBook("/book.epub", "title", "author", "epub"));
  offset = 0;
  assert(s.nextDeletion(offset, id) && !id[0]);
  assert(s.applyRemote(remote, "remote", false));
  s.clearAll();
  s.unload();
  assert(s.loadForBook("/book.epub", "title", "author", "epub"));
  assert(!s.hasClippings());
  std::vector<ClippedBookEntry> books;
  assert(ClippingStore::getAllClippedBooks(books) && books.empty());
  std::puts("migration v1-v4 owner + incompatible modern v4; v5 offsets/revisions/IDs/tombstones/clear roundtrip PASS");

  for (bool alias : {false, true})
    for (int failure : {0, 1}) {
      reset();
      ClippingStore x;
      assert(x.loadForBook("/book.epub", "title", "author", "epub"));
      assert(add(x) == Result::Added);
      const auto primary = path();
      const auto old = fake::files.at(primary)->bytes;
      fake::aliasRenameFailure = alias;
      fake::failRename = failure;
      assert(add(x, "second") == Result::SaveFailed);
      assert(fake::failureTriggered);
      assert(fake::files.contains(primary + ".tmp"));
      bool oldRetained = false;
      for (auto& [n, node] : fake::files)
        if (node->bytes == old) oldRetained = true;
      assert(oldRetained);
      const auto count = fake::writesByPath;
      assert(add(x, "third") == Result::SaveFailed);
      assert(fake::writesByPath == count);
      ClippingStore reopened;
      assert(!reopened.loadForBook("/book.epub", "title", "author", "epub"));
      assert(!reopened.prepareSync());
      assert(!reopened.saveToFile());
    }
  std::puts(
      "negative rename before/after publication with shared-node FAT aliases: retain roots + reboot/retry refusal "
      "PASS");

  for (int boundary = 0; boundary < 64; ++boundary) {
    reset();
    ClippingStore x;
    assert(x.loadForBook("/book.epub", "title", "author", "epub"));
    assert(add(x) == Result::Added);
    const auto primary = path();
    const auto old = fake::files.at(primary)->bytes;
    fake::failWrite = boundary;
    auto result = add(x, "second");
    if (fake::failureTriggered) {
      assert(result == Result::SaveFailed);
      assert(fake::files.at(primary)->bytes == old);
    }
  }
  reset();
  ClippingStore x;
  assert(x.loadForBook("/book.epub", "title", "author", "epub"));
  assert(add(x) == Result::Added);
  p = path();
  auto old = fake::files.at(p)->bytes;
  fake::failSync = 0;
  assert(add(x, "second") == Result::SaveFailed);
  assert(fake::files.at(p)->bytes == old);
  fake::files.at(p)->bytes.pop_back();
  ClippingStore broken;
  assert(!broken.loadForBook("/book.epub", "title", "author", "epub"));
  assert(broken.clippingCount() == 0);
  assert(!broken.prepareSync());
  std::puts("bounded POD/string/text write faults, sync fault and truncated-load write denial PASS");

  reset();
  fake::add("/book.epub");
  ClippingStore move;
  assert(move.loadForBook("/book.epub", "title", "author", "epub"));
  assert(add(move) == Result::Added);
  assert(move.prepareSync());
  const std::string movedId = move.clippingAt(0)->id;
  move.unload();
  assert(ClippingStore::moveBook("/book.epub", "/new.epub"));
  assert(Storage.exists("/new.epub") && !Storage.exists("/book.epub"));
  assert(move.loadForBook("/new.epub", "title", "author", "epub"));
  assert(movedId == move.clippingAt(0)->id);
  std::puts("book rename copies versioned metadata with identity before physical move PASS");
  unsigned corruptions = 0;
  for (int boundary = 0; boundary < 64; ++boundary) {
    reset();
    ClippingStore x;
    assert(x.loadForBook("/book.epub", "title", "author", "epub"));
    assert(add(x) == Result::Added);
    const auto primary = path();
    const auto old = fake::files.at(primary)->bytes;
    fake::corruptWrite = boundary;
    auto result = add(x, "second");
    if (fake::failureTriggered) {
      ++corruptions;
      assert(result == Result::SaveFailed);
      assert(fake::files.at(primary)->bytes == old);
    }
  }
  assert(corruptions > 30);
  reset();
  ClippingStore closeFailure;
  assert(closeFailure.loadForBook("/book.epub", "title", "author", "epub"));
  assert(add(closeFailure) == Result::Added);
  p = path();
  old = fake::files.at(p)->bytes;
  fake::failClosePath = p + ".tmp";
  assert(add(closeFailure, "second") == Result::SaveFailed);
  assert(fake::files.at(p)->bytes == old);
  std::printf(
      "acknowledged corruption at %u positive-write boundaries + temp-close fault rejected by exact snapshot readback "
      "PASS\n",
      corruptions);

  // Imported modern hash-path + journal survive v5 migration and acknowledged clear.
  reset();
  const std::string modernPath =
      "/.crosspoint/clippings/epub_" + std::to_string(std::hash<std::string>{}("/book.epub")) + ".bin";
  legacy(modernPath, 4, true);
  fake::add(modernPath + ".deleted", std::string("feed", 4) + std::string(61, '\0'));
  ClippingStore imported;
  assert(imported.loadForBook("/book.epub", "title", "author", "epub"));
  offset = 0;
  assert(imported.nextDeletion(offset, id) && std::string(id) == "feed");
  assert(imported.prepareSync());
  assert(fake::files.at(modernPath)->bytes[0] == 5);
  assert(imported.finishDeletions());
  imported.unload();
  assert(imported.loadForBook("/book.epub", "title", "author", "epub"));
  offset = 0;
  assert(imported.nextDeletion(offset, id) && !id[0]);
  std::puts("modern filename/journal migration + acknowledged tombstone non-resurrection PASS");

  // A failed physical move is indeterminate: durable marker prevents reopen,
  // retries, and writes, even if a driver published a second alias.
  reset();
  fake::add("/book.epub");
  ClippingStore uncertain;
  assert(uncertain.loadForBook("/book.epub", "title", "author", "epub"));
  assert(add(uncertain) == Result::Added);
  fake::failRename = 1;
  fake::aliasRenameFailure = true;
  assert(!ClippingStore::moveBook("/book.epub", "/new.epub"));
  assert(Storage.exists("/.crosspoint/clippings/move.pending"));
  ClippingStore afterMove;
  assert(!afterMove.loadForBook("/book.epub", "title", "author", "epub"));
  assert(add(uncertain) == Result::SaveFailed);
  assert(!ClippingStore::moveBook("/book.epub", "/other.epub"));
  std::puts("durable move intent + uncertain physical rename blocks reboot/retry writers PASS");
}
