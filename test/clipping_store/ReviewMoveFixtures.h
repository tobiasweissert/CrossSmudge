#pragma once
#include <ClippingStore.h>
#include <HalStorage.h>
#include <Serialization.h>
#include <gtest/gtest.h>

namespace reviewMove {
inline std::map<std::string, std::vector<uint8_t>> bytes() {
  std::map<std::string, std::vector<uint8_t>> result;
  for (const auto& [path, node] : fake::files) result[path] = node->bytes;
  return result;
}
inline std::string legacy(const std::string& book, size_t tail) {
  ClippingStore seed;
  EXPECT_TRUE(seed.loadForBook(book, "Legacy title", "Legacy author", "epub"));
  EXPECT_EQ(seed.addClipping(0, 0, 0, 1, 0, 0, 1, "Chapter", 0, "quote", uint32_t{1}), ClippingStore::AddResult::Added);
  seed.unload();
  std::string path;
  for (const auto& [p, n] : fake::files)
    if (p.ends_with(".bin")) path = p;
  HalFile f = Storage.open(path.c_str(), O_WRONLY | O_TRUNC);
  EXPECT_TRUE(serialization::tryWritePod(f, uint8_t{3}));
  EXPECT_TRUE(serialization::tryWritePod(f, uint16_t{1}));
  for (const auto& s : {std::string("Legacy title"), std::string("Legacy author"), book})
    EXPECT_TRUE(serialization::tryWriteString(f, s));
  for (uint16_t n :
       {uint16_t{0}, uint16_t{0}, uint16_t{0}, uint16_t{1}, uint16_t{0}, uint16_t{0}, uint16_t{1}, uint16_t{0}})
    EXPECT_TRUE(serialization::tryWritePod(f, n));
  EXPECT_TRUE(serialization::tryWritePod(f, uint32_t{123}));
  EXPECT_TRUE(serialization::tryWritePod(f, uint32_t{456}));
  char chapter[48] = "Chapter";
  EXPECT_EQ(f.write(chapter, sizeof(chapter)), sizeof(chapter));
  EXPECT_TRUE(serialization::tryWritePod(f, uint16_t{5}));
  EXPECT_EQ(f.write("quote", 5), 5u);
  EXPECT_TRUE(f.close());
  std::string raw(65, '\0');
  raw.replace(0, 7, "deleted");
  raw.append(tail, 'x');
  fake::add(path + ".deleted", raw);
  return path;
}
template <typename Move>
void legacyMove(Move move, bool folder) {
  for (size_t tail = 1; tail <= 64; ++tail) {
    SCOPED_TRACE(tail);
    fake::reset();
    const std::string book = folder ? "/folder/book.epub" : "/book.epub";
    const std::string from = folder ? "/folder" : book;
    const std::string to = folder ? "/renamed" : "/new.epub";
    const std::string movedBook = folder ? "/renamed/book.epub" : to;
    if (folder) Storage.mkdir(from.c_str());
    fake::add(book, "entire physical book");
    const auto path = legacy(book, tail);
    const auto raw = fake::files.at(path + ".deleted")->bytes;
    for (bool published : {false, true}) {
      SCOPED_TRACE(published);
      ClippingStore s;
      ASSERT_TRUE(s.loadForBook(book, "", "", "epub"));
      std::string text;
      ASSERT_TRUE(s.readClippingText(0, text));
      EXPECT_EQ(text, "quote");
      if (published) {
        ASSERT_TRUE(s.prepareSync());
      }
      const auto original = bytes();
      const auto writes = fake::writesByPath;
      EXPECT_FALSE(move(from, to));
      EXPECT_EQ(bytes(), original);
      EXPECT_EQ(fake::writesByPath, writes);
      EXPECT_FALSE(Storage.exists("/.crosspoint/book-move.pending"));
      EXPECT_FALSE(Storage.exists("/.crosspoint/clippings/move.pending"));
      ASSERT_TRUE(Storage.exists((path + ".deleted").c_str()));
      EXPECT_EQ(fake::files.at(path + ".deleted")->bytes, raw);
      s.unload();  // Simulated reboot: v5 is durable, raw journal is still retained.
    }
    ClippingStore acknowledge;
    ASSERT_TRUE(acknowledge.loadForBook(book, "", "", "epub"));
    fake::failRemovePath = path + ".deleted";
    fake::failureTriggered = false;
    EXPECT_FALSE(acknowledge.finishDeletions());
    ASSERT_TRUE(fake::failureTriggered);
    EXPECT_EQ(fake::files.at(path + ".deleted")->bytes, raw);
    const auto failedAck = bytes();
    EXPECT_FALSE(move(from, to));
    EXPECT_EQ(bytes(), failedAck);
    ASSERT_TRUE(acknowledge.finishDeletions());
    acknowledge.unload();
    ASSERT_FALSE(Storage.exists((path + ".deleted").c_str()));
    ASSERT_TRUE(move(from, to));
    ASSERT_TRUE(Storage.exists(movedBook.c_str()));
    ASSERT_TRUE(acknowledge.loadForBook(movedBook, "", "", "epub"));
    std::string text;
    ASSERT_TRUE(acknowledge.readClippingText(0, text));
    EXPECT_EQ(text, "quote");
    acknowledge.unload();
    EXPECT_EQ(fake::liveCheckedAllocations, 0u);
  }
}
template <typename Move>
void cleanTombstones(Move move, bool folder) {
  fake::reset();
  const std::string book = folder ? "/folder/book.epub" : "/book.epub";
  const std::string from = folder ? "/folder" : book;
  const std::string to = folder ? "/renamed" : "/new.epub";
  const std::string movedBook = folder ? "/renamed/book.epub" : to;
  if (folder) Storage.mkdir(from.c_str());
  fake::add(book, "entire physical book");
  ClippingStore s;
  ASSERT_TRUE(s.loadForBook(book, "Book", "Author", "epub"));
  for (unsigned i = 0; i < 2; ++i)
    ASSERT_EQ(s.addClipping(0, 0, 0, 1, 0, 0, 1, "Chapter", 0, "quote", uint32_t{1}), ClippingStore::AddResult::Added);
  ASSERT_TRUE(s.prepareSync());
  const std::string deletedId = s.clippingAt(0)->id;
  ASSERT_TRUE(s.removeClippingAt(0));
  s.unload();
  ASSERT_TRUE(move(from, to));
  ASSERT_TRUE(s.loadForBook(movedBook, "", "", "epub"));
  ASSERT_EQ(s.clippingCount(), 1u);
  std::string text;
  ASSERT_TRUE(s.readClippingText(0, text));
  EXPECT_EQ(text, "quote");
  uint32_t offset = 0;
  char id[65];
  ASSERT_TRUE(s.nextDeletion(offset, id));
  EXPECT_STREQ(id, deletedId.c_str());
  s.unload();
  EXPECT_EQ(fake::liveCheckedAllocations, 0u);
}
}  // namespace reviewMove
