#include <Epub.h>
#include <GfxRenderer.h>
#include <Epub/Page.h>
#include <Epub/ParsedText.h>
#include <Epub/InlineImageToken.h>
#include <Epub/parsers/ChapterHtmlSlimParser.h>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace {
struct Layout {
  GfxRenderer renderer;
  std::shared_ptr<Epub> book = std::make_shared<Epub>();
  std::vector<std::unique_ptr<Page>> pages;
  std::string path = "/root/.hermes/cache/scratch/epub-layout-final-fixture.xhtml";
  bool parse(const std::string& body, int width = 160, int height = 120, int imageHeight = 16) {
    std::ofstream(path) << "<html><body>" << body << "</body></html>";
    Epub::enableGifProbeForTest = true;
    book->imageBytes = {'G','I','F','8','9','a',16,0,static_cast<uint8_t>(imageHeight),0};
    ChapterHtmlSlimParser parser(book,path,renderer,0,1,false,0,width,height,false,false,
      [&](std::unique_ptr<Page> page,auto,auto,auto){ pages.push_back(std::move(page)); },true,"","/root/.hermes/cache/scratch/epub-layout-final-r2/icon_");
    parser.setParagraphIndentSpaces(0);
    return parser.parseAndBuildPages();
  }
  ~Layout() { std::filesystem::remove(path); Epub::enableGifProbeForTest = false; }
};
std::string text(const Page& page) {
  std::string result;
  page.forEachTextLine([](const PageTextLine& line, void* context) {
    auto& result = *static_cast<std::string*>(context);
    for (uint16_t i=0;i<line.block->wordCount();++i) {
      if (line.block->wordHasSpaceBefore(i)) result += ' ';
      result += line.block->wordText(i);
    }
    return true;
  }, &result);
  return result;
}
}
TEST(Migration, InlineImageParticipatesAndNeverSerializesToken) {
  Layout l; ASSERT_TRUE(l.parse("<p>Alpha<img src='icon.gif'/>Beta</p>"));
  ASSERT_EQ(l.pages.size(),1u); EXPECT_TRUE(l.pages[0]->hasImages());
  EXPECT_EQ(text(*l.pages[0]),"AlphaBeta");
  const auto path = "/root/.hermes/cache/scratch/epub-layout-final-page.bin";
  HalFile out; ASSERT_TRUE(Storage.openFileForWrite("test",path,out)); ASSERT_TRUE(l.pages[0]->serialize(out)); out.close();
  std::ifstream bytes(path,std::ios::binary); std::string raw((std::istreambuf_iterator<char>(bytes)),{});
  EXPECT_EQ(raw.find(std::string("\x01I")), std::string::npos);
  HalFile in; ASSERT_TRUE(Storage.openFileForRead("test",path,in)); auto warm=Page::deserialize(in);
  ASSERT_TRUE(warm); EXPECT_TRUE(warm->hasImages()); EXPECT_EQ(text(*warm),"AlphaBeta");
  in.close(); std::filesystem::remove(path);
}
TEST(Migration, InlineImageWrapsAndRaisesTextBaseline) {
  Layout l; ASSERT_TRUE(l.parse("<p>Alpha<img style='display:inline' src='icon.gif'/>Beta</p>",64,120,32));
  ASSERT_EQ(l.pages.size(),1u);
  const PageImage* image=nullptr; const PageLine* alpha=nullptr; const PageLine* beta=nullptr;
  for (const auto& e:l.pages[0]->elements) {
    if(e->getTag()==TAG_PageImage) image=static_cast<const PageImage*>(e.get());
    if(e->getTag()==TAG_PageLine) { auto* line=static_cast<const PageLine*>(e.get());
      for(uint16_t i=0;i<line->getBlock()->wordCount();++i) {
        if(std::string(line->getBlock()->wordText(i))=="Alpha") alpha=line;
        if(std::string(line->getBlock()->wordText(i))=="Beta") beta=line;
      }
    }
  }
  ASSERT_TRUE(image); ASSERT_TRUE(alpha); ASSERT_TRUE(beta);
  EXPECT_GT(beta->yPos,alpha->yPos); EXPECT_GE(beta->yPos,image->yPos);
}
TEST(Migration, ExplicitBlockStaysBetweenTextLines) {
  Layout l; ASSERT_TRUE(l.parse("<p>Alpha<img style='display:block' src='icon.gif'/>Beta</p>"));
  ASSERT_EQ(l.pages.size(),1u);
  std::vector<int> y;
  for(const auto& e:l.pages[0]->elements) if(e->getTag()==TAG_PageLine || e->getTag()==TAG_PageImage) y.push_back(e->yPos);
  ASSERT_GE(y.size(),3u); EXPECT_LT(y[0],y[1]); EXPECT_LT(y[1],y[2]);
}
TEST(Migration, IconOnlyRunRemainsBoundedAndAllImagesSurvive) {
  Layout l; std::string body="<p>";
  for(int i=0;i<50;++i) body += "<img src='icon.gif'/>";
  body += "</p>"; ASSERT_TRUE(l.parse(body));
  size_t images=0;
  for(const auto& p:l.pages) { EXPECT_TRUE(text(*p).empty()); for(const auto& e:p->elements) images+=e->getTag()==TAG_PageImage; }
  EXPECT_EQ(images,50u);
}
TEST(Migration, SourcePreservingLayoutCanRetryAtAnotherWidth) {
  GfxRenderer r; ParsedText p(false,false,BlockStyle{},0);
  p.addWord("Alpha",EpdFontFamily::REGULAR,false,false,10);
  p.addWord("Beta",EpdFontFamily::REGULAR,false,false,16);
  for(int width : {48,160}) {
    std::string got;
    ASSERT_TRUE(p.layoutAndExtractLinesPreservingSource(r,0,width,[&](std::unique_ptr<TextBlock> line){
      for(uint16_t i=0;i<line->wordCount();++i) { got += line->wordText(i); EXPECT_GE(line->wordSourceRange(i).start,10u); }
    }));
    EXPECT_EQ(got,"AlphaBeta"); EXPECT_EQ(p.size(),2u);
  }
}
TEST(Migration, PackedPresentationSurvivesWarmPage) {
  GfxRenderer r; ParsedText p(false,false,BlockStyle{},0);p.setGuideReading(true);p.setBackgroundBlack(true);
  const auto slot=p.sizeSlotFor(1.5f);p.setSizeSlotFontId(slot,123);
  p.addWord("Alpha",EpdFontFamily::REGULAR,false,false,10,0,slot);p.setBackgroundBlack(false);
  p.addWord("Beta",EpdFontFamily::REGULAR,false,false,16);
  p.layoutAndExtractLines(r,0,240,[&](std::unique_ptr<TextBlock> b,uint32_t){
    ASSERT_EQ(b->wordCount(),2);ASSERT_GT(b->guideDotXOffset(0),0);
    const auto dot=b->guideDotXOffset(0);const auto range=b->wordSourceRange(0);
    Page page;page.elements.push_back(std::make_unique<PageLine>(std::move(b),0,0));
    const char* path="/root/.hermes/cache/scratch/epub-layout-final-r2/presentation.bin";
    HalFile out;ASSERT_TRUE(Storage.openFileForWrite("test",path,out));ASSERT_TRUE(page.serialize(out));out.close();
    HalFile in;ASSERT_TRUE(Storage.openFileForRead("test",path,in));auto cached=Page::deserialize(in);ASSERT_TRUE(cached);
    const auto* warm=static_cast<PageLine*>(cached->elements[0].get())->getBlock();
    EXPECT_EQ(warm->guideDotXOffset(0),dot);EXPECT_TRUE(warm->wordFlags(0)&TextBlock::WORD_FLAG_BACKGROUND_BLACK);
    EXPECT_FALSE(warm->wordFlags(1)&TextBlock::WORD_FLAG_BACKGROUND_BLACK);EXPECT_FALSE(warm->wordHasSpaceBefore(0));EXPECT_TRUE(warm->wordHasSpaceBefore(1));
    EXPECT_EQ(warm->wordSourceRange(0).start,range.start);EXPECT_EQ(warm->wordSourceRange(0).end,range.end);
    EXPECT_EQ(warm->wordSourceRange(1).start,16u);EXPECT_EQ(warm->wordFontId(0,0),123);EXPECT_EQ(warm->wordFontId(1,0),0);
    in.close();std::filesystem::remove(path);
  });
}
TEST(Migration, TableColumnsAndClippingSurviveWarmPage) {
  Layout l; ASSERT_TRUE(l.parse("<table><tr><td>Alpha</td><td>Beta</td></tr><tr><td>Gamma</td><td>Delta</td></tr></table>",240,200));
  ASSERT_EQ(l.pages.size(),1u);
  auto verify=[](const Page& p) {
    std::vector<PageTextLine> lines;
    p.forEachTextLine([](const PageTextLine& line,void* out){ static_cast<std::vector<PageTextLine>*>(out)->push_back(line);return true;},&lines);
    ASSERT_EQ(lines.size(),4u);
    EXPECT_TRUE(lines[0].isTableText); EXPECT_GT(lines[0].clipWidth,0);
    EXPECT_NE(lines[0].tableSelection,lines[1].tableSelection);
    EXPECT_EQ(lines[0].tableSelection,lines[2].tableSelection);
    EXPECT_EQ(lines[1].tableSelection,lines[3].tableSelection);
  };
  verify(*l.pages[0]);
  const char* path="/root/.hermes/cache/scratch/epub-layout-final-table.bin";
  HalFile out; ASSERT_TRUE(Storage.openFileForWrite("test",path,out)); ASSERT_TRUE(l.pages[0]->serialize(out));out.close();
  HalFile in; ASSERT_TRUE(Storage.openFileForRead("test",path,in));auto warm=Page::deserialize(in);ASSERT_TRUE(warm);verify(*warm);
  in.close();std::filesystem::remove(path);
}
TEST(Migration, GuideDotsReserveSpaceAndWarmPresentationIsExact) {
  GfxRenderer r; ParsedText p(false,false,BlockStyle{},0);p.setGuideReading(true);p.setBackgroundBlack(true);
  p.addWord("Alpha",EpdFontFamily::REGULAR);p.addWord("Beta",EpdFontFamily::REGULAR);
  p.layoutAndExtractLines(r,0,160,[&](std::unique_ptr<TextBlock> b,uint32_t){
    ASSERT_EQ(b->wordCount(),2);ASSERT_GT(b->guideDotXOffset(0),0);
    const auto dot=b->guideDotXOffset(0);
    Page page;page.elements.push_back(std::make_unique<PageLine>(std::move(b),0,0));
    const char* path="/root/.hermes/cache/scratch/epub-layout-final-style.bin";
    HalFile out;ASSERT_TRUE(Storage.openFileForWrite("test",path,out));ASSERT_TRUE(page.serialize(out));out.close();
    HalFile in;ASSERT_TRUE(Storage.openFileForRead("test",path,in));auto warm=Page::deserialize(in);ASSERT_TRUE(warm);
    auto* block=static_cast<PageLine*>(warm->elements[0].get())->getBlock();
    EXPECT_EQ(block->guideDotXOffset(0),dot);EXPECT_TRUE(block->wordFlags(0)&TextBlock::WORD_FLAG_BACKGROUND_BLACK);
    in.close();std::filesystem::remove(path);
  });
}
#include <MemoryBudget.h>
TEST(Migration, LowHeapFailsWithoutPublishingPartialText) {
  Layout l; MemoryBudget::textHeapAvailable=false;
  const bool parsed=l.parse("<p>Alpha Beta Gamma</p>");
  MemoryBudget::textHeapAvailable=true;
  EXPECT_FALSE(parsed);EXPECT_TRUE(l.pages.empty());
}
TEST(Migration, GeneralEntitiesCannotExpandIntoChapterText) {
  Layout l; std::ofstream(l.path)<<"<!DOCTYPE html [<!ENTITY test 'Expanded'>]><html><body><p>&test;</p></body></html>";
  ChapterHtmlSlimParser p(l.book,l.path,l.renderer,0,1,false,0,160,120,false,false,
    [&](std::unique_ptr<Page> page,auto,auto,auto){l.pages.push_back(std::move(page));},true,"","");
  EXPECT_FALSE(p.parseAndBuildPages());EXPECT_TRUE(l.pages.empty());
}
TEST(Migration, LegacySmallCapsAndInsertedHyphenKeepDistinctBits) {
  const auto cap=static_cast<EpdFontFamily::Style>(EpdFontFamily::SMALL_CAPS|EpdFontFamily::BOLD);
  TextBlock block({"Small","split-"},{0,48},{cap,EpdFontFamily::REGULAR},{},{},{},{0,TextBlock::WORD_FLAG_INSERTED_HYPHEN},{false,true});
  ASSERT_TRUE(block.valid());EXPECT_TRUE(block.wordHasSmallCaps(0));EXPECT_FALSE(block.wordHasDiscretionaryHyphen(0));
  EXPECT_TRUE(block.wordHasDiscretionaryHyphen(1));EXPECT_FALSE(block.wordHasSmallCaps(1));
}
TEST(Migration, MalformedChapterKeepsPrefixAndDisclosesTruncation) {
  Layout l;ASSERT_TRUE(l.parse("<p>Preserved prefix</p><p>Malformed</body>"));
  std::string all;
  for(const auto& page:l.pages) all+=text(*page);
  EXPECT_NE(all.find("Preserved"),std::string::npos);
  EXPECT_NE(all.find("Chapter incomplete"),std::string::npos);
}
TEST(Migration, PublisherBackgroundEndsWithItsSpan) {
  Layout l;ASSERT_TRUE(l.parse("<p><span style='background-color:black'>Black</span> Normal</p>"));
  bool black=false,normal=false;
  for(const auto& p:l.pages) for(const auto& e:p->elements) if(e->getTag()==TAG_PageLine) {
    const auto* b=static_cast<PageLine*>(e.get())->getBlock();
    for(uint16_t i=0;i<b->wordCount();++i) {
      if(std::string(b->wordText(i))=="Black") {black=true;EXPECT_TRUE(b->wordFlags(i)&TextBlock::WORD_FLAG_BACKGROUND_BLACK);}
      if(std::string(b->wordText(i))=="Normal") {normal=true;EXPECT_FALSE(b->wordFlags(i)&TextBlock::WORD_FLAG_BACKGROUND_BLACK);}
    }
  }
  EXPECT_TRUE(black);EXPECT_TRUE(normal);
}
TEST(Migration, OwnerSpacingAddsPixelsAlongsidePercentageSpacing) {
  GfxRenderer r;
  int x[2]={};
  for(int mode=0;mode<2;++mode) {
    ParsedText p(false,false,BlockStyle{},0);p.setLegacyWordSpacing(mode);
    p.addWord("Alpha",EpdFontFamily::REGULAR);p.addWord("Beta",EpdFontFamily::REGULAR);
    p.layoutAndExtractLines(r,0,240,[&](std::unique_ptr<TextBlock> b,uint32_t){ASSERT_EQ(b->wordCount(),2);x[mode]=b->wordXpos(1);},true,0,150);
  }
  EXPECT_EQ(x[1]-x[0],10);
}
TEST(Migration, FootnoteIdentitiesRemainDistinctAcrossParagraphs) {
  Layout l;ASSERT_TRUE(l.parse("<p><a href='#one'>One</a></p><p><a href='#two'>Two</a></p>",160,200));
  ASSERT_EQ(l.pages.size(),1u);ASSERT_EQ(l.pages[0]->footnotes.size(),2u);
  const auto& notes=l.pages[0]->footnotes;EXPECT_NE(notes[0].linkId,notes[1].linkId);
  for(const auto& e:l.pages[0]->elements) if(e->getTag()==TAG_PageLine) {
    const auto* b=static_cast<PageLine*>(e.get())->getBlock();
    for(uint16_t i=0;i<b->wordCount();++i) {
      const auto id=b->wordLinkId(i);ASSERT_NE(id,0);
      const auto found=std::find_if(notes.begin(),notes.end(),[&](const auto& note){return note.linkId==id;});
      ASSERT_NE(found,notes.end());EXPECT_EQ(std::string(found->number),b->wordText(i));
    }
  }
}
TEST(Migration, TallInlineImageMovesLinkHitRectangleWithText) {
  Layout l;ASSERT_TRUE(l.parse("<p><a href='#note'>Alpha</a><img style='display:inline' src='icon.gif'/></p>",160,120,32));
  ASSERT_EQ(l.pages.size(),1u);ASSERT_EQ(l.pages[0]->links.size(),1u);
  const PageLine* line=nullptr;
  for(const auto& e:l.pages[0]->elements) if(e->getTag()==TAG_PageLine) line=static_cast<PageLine*>(e.get());
  ASSERT_TRUE(line);EXPECT_GT(line->yPos,0);EXPECT_EQ(l.pages[0]->links[0].y,line->yPos);
}
TEST(Migration, OwnerPublisherIndentSurvivesNumericDefaultOff) {
  GfxRenderer r; BlockStyle style;style.alignment=CssTextAlign::Left;style.textIndentDefined=true;style.textIndent=24;
  ParsedText p(false,false,false,false,false,0,style);p.addWord("Alpha",EpdFontFamily::REGULAR);
  p.layoutAndExtractLines(r,0,160,[](std::unique_ptr<TextBlock> b,uint32_t){ASSERT_EQ(b->wordCount(),1);EXPECT_EQ(b->wordXpos(0),24);});
  Layout l;ASSERT_TRUE(l.parse("<p style='text-indent:2em'>Alpha</p>"));
  const PageLine* line=nullptr;
  for(const auto& e:l.pages[0]->elements) if(e->getTag()==TAG_PageLine) line=static_cast<PageLine*>(e.get());
  ASSERT_TRUE(line);EXPECT_GT(line->getBlock()->wordXpos(0),0);
}
TEST(Migration, ImageAtParagraphStartPreservesFirstTextBoundary) {
  Layout l;ASSERT_TRUE(l.parse("<p><img src='icon.gif'/>Alpha</p>"));
  bool found=false;
  for(const auto& page:l.pages) for(const auto& e:page->elements) if(e->getTag()==TAG_PageLine) {
    const auto* b=static_cast<const PageLine*>(e.get())->getBlock();ASSERT_GT(b->wordCount(),0);
    if(std::string(b->wordText(0))=="Alpha") {found=true;EXPECT_TRUE(b->wordStartsParagraph(0));}
  }
  EXPECT_TRUE(found);
}
TEST(Migration, PreviewCompletionAfterParserTeardownIsSafe) {
  Layout l;std::ofstream(l.path)<<"<html><body><p id='target'>Alpha</p></body></html>";
  ChapterHtmlSlimParser p(*l.book,l.path,l.renderer,0,1,false,false,0,160,120,false,false,false,0,
    [&](std::unique_ptr<Page> page,auto,auto,auto,auto){l.pages.push_back(std::move(page));},true,"","",0,{},nullptr,nullptr,
    EpubRenderMode::CrossSmudgeDefault,"target",1);
  EXPECT_TRUE(p.parseAndBuildPages());ASSERT_EQ(l.pages.size(),1u);EXPECT_EQ(text(*l.pages[0]),"Alpha");
}
TEST(Migration, ExplicitImageDisplayHonorsDeclarationOrderAndImportance) {
  for(const std::string style : {"display:inline;display:block", "--display:inline;display:block"}) {
    Layout l;ASSERT_TRUE(l.parse("<p>Alpha<img style='"+style+"' src='icon.gif'/>Beta</p>"));
    const PageImage* image=nullptr;const PageLine* first=nullptr;
    for(const auto& e:l.pages[0]->elements) {
      if(e->getTag()==TAG_PageImage)image=static_cast<PageImage*>(e.get());
      if(!first && e->getTag()==TAG_PageLine)first=static_cast<PageLine*>(e.get());
    }
    ASSERT_TRUE(image);ASSERT_TRUE(first);EXPECT_GT(image->yPos,first->yPos);
  }
  for(const std::string style : {"DISPLAY: INLINE", "display:inline!important;display:block"}) {
    Layout l;ASSERT_TRUE(l.parse("<p>Alpha<img style='"+style+"' src='icon.gif'/></p>",160,120,32));
    const PageImage* image=nullptr;const PageLine* first=nullptr;
    for(const auto& e:l.pages[0]->elements) {
      if(e->getTag()==TAG_PageImage)image=static_cast<PageImage*>(e.get());
      if(!first && e->getTag()==TAG_PageLine)first=static_cast<PageLine*>(e.get());
    }
    ASSERT_TRUE(image);ASSERT_TRUE(first);EXPECT_GT(first->yPos,image->yPos);
  }
}
