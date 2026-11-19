#include <gtest/gtest.h>
#include "LibraryBuilder.h"
#include "LibraryMetadataCache.h"
#include "LibraryIndexFile.h"
#include "ZipFile.h"
using namespace library;
static bool read(std::string& title,std::string& author,std::string& series,std::string& genre,std::string& position) {
 return readLibraryMetadataFromSource("/test.epub",title,author,series,genre,position);
}
class SourceMetadataTest:public ::testing::Test {
protected:
 std::string title,author,series,genre,position;
 void SetUp()override{zipEntries.clear();fake::reset();zipEntries["/test.epub"]["META-INF/container.xml"]="<container><rootfiles><rootfile full-path=\"book.opf\"/></rootfiles></container>";}
 bool parse(const std::string& xml){zipEntries["/test.epub"]["book.opf"]=xml;return read(title,author,series,genre,position);}
};
TEST_F(SourceMetadataTest, ParsesRealExpatCalibreMetadataAndStopsBeforeManifest) {
 ASSERT_TRUE(parse("<package xmlns:dc=\"urn:dc\"><metadata><dc:title>Title &amp; more</dc:title><dc:creator>A Writer</dc:creator><dc:subject>Fantasy</dc:subject><meta name=\"calibre:series\" content=\"Sequence\"/><meta name=\"calibre:series_index\" content=\"2.5\"/></metadata><invalid-manifest"));
 EXPECT_EQ(title,"Title & more");EXPECT_EQ(author,"A Writer");EXPECT_EQ(series,"Sequence");EXPECT_EQ(genre,"Fantasy");EXPECT_EQ(position,"2.5");
}
TEST_F(SourceMetadataTest, ParsesEpubThreeCollection) {
 ASSERT_TRUE(parse("<package><metadata><meta property=\"belongs-to-collection\" id=\"c\">Saga</meta><meta refines=\"#c\" property=\"collection-type\">series</meta><meta property=\"group-position\" refines=\"#c\">3</meta></metadata></package>"));
 EXPECT_EQ(series,"Saga");EXPECT_EQ(position,"3");
}
TEST_F(SourceMetadataTest, PreservesMultipleCreatorsAndOutOfOrderCollectionRefinements) {
 ASSERT_TRUE(parse("<package xmlns:dc=\"urn:dc\"><metadata><dc:creator> First   Writer </dc:creator><dc:creator>Second Writer</dc:creator><meta property=\"belongs-to-collection\" id=\"a\">Other collection</meta><meta refines=\"#a\" property=\"collection-type\">set</meta><meta refines=\"#b\" property=\"group-position\">4</meta><meta refines=\"#b\" property=\"collection-type\"> series </meta><meta property=\"belongs-to-collection\" id=\"b\">Real Saga</meta></metadata></package>"));
 EXPECT_EQ(author,"First Writer, Second Writer");EXPECT_EQ(series,"Real Saga");EXPECT_EQ(position,"4");
}
TEST_F(SourceMetadataTest, RejectsDtdAndTraversalAndTruncatedXml) {
 EXPECT_FALSE(parse("<!DOCTYPE package [<!ENTITY secret SYSTEM 'file:///private'>]><package><metadata><title>&secret;</title></metadata></package>"));
 EXPECT_FALSE(parse("<package><metadata><title>unfinished"));
 zipEntries["/test.epub"]["META-INF/container.xml"]="<container><rootfile full-path=\"../book.opf\"/></container>";
 EXPECT_FALSE(parse("<package><metadata/></package>"));
}
TEST_F(SourceMetadataTest, RejectsDeepAndOversizedMetadataWithoutPublishingValues) {
 std::string deep="<package><metadata>";for(int i=0;i<35;++i)deep+="<a>";EXPECT_FALSE(parse(deep));
 EXPECT_FALSE(parse("<package><metadata><title>"+std::string(270000,'A')+"</title></metadata></package>"));
 EXPECT_TRUE(title.empty());
}
TEST_F(SourceMetadataTest, XtcMetadataIsRetainedFromUpstream) {
 fake::add("/book.xtc");BuildStats stats;ASSERT_TRUE(buildLibraryIndex("/",stats,true));
 LibraryIndexFile idx;ASSERT_TRUE(idx.open(libraryIndexPath()));ClixRecord rec{};ASSERT_TRUE(idx.readRecord(0,rec));
 ASSERT_TRUE(idx.readTitle(rec,title));ASSERT_TRUE(idx.readAuthor(rec,author));EXPECT_EQ(title,"XTC title");EXPECT_EQ(author,"XTC author");
}
