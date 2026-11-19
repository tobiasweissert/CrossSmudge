#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "ContentOpfParser.h"
#include "Epub/BookMetadataCache.h"
#include "ZipFile.h"

static void feed(ContentOpfParser& p, const std::string& xml, size_t step) {
  assert(p.setup());
  for(size_t i=0;i<xml.size();i+=step) {
    const auto n=std::min(step,xml.size()-i);
    assert(p.write(reinterpret_cast<const uint8_t*>(xml.data()+i),n)==n);
  }
}
int main() {
  const std::string root="/book", empty="";
  const std::string xml=R"(<package xmlns:dc="urn:dc"><metadata><dc:title> One &#38; Two </dc:title><dc:creator>Émile Zola</dc:creator><dc:creator>Ursula Le Guin</dc:creator><dc:identifier>urn:isbn:978123</dc:identifier><meta name="calibre:series" content="Series"/><meta name="calibre:series_index" content="2.5"/></metadata><manifest><item id="ch" href="ch.xhtml" media-type="application/xhtml+xml"/><item id="css" href="book.css" media-type="text/css"/></manifest><spine><itemref idref="ch"/></spine><guide><reference type="text" href="wrong.xhtml"/><reference type="start" href="ch.xhtml#start"/><reference type="toc" href="toc.xhtml#toc"/></guide></package>)";
  for(size_t step: {size_t(1),size_t(7),size_t(1024)}) {
    Storage.reset();
    BookMetadataCache cache("/book",true);
    assert(cache.beginWrite());assert(cache.beginContentOpfPass());
    {
      ContentOpfParser p(root,empty,xml.size(),&cache);
      feed(p,xml,step);
      assert(p.title=="One & Two");assert(p.author=="Émile Zola, Ursula Le Guin");
      assert(p.isbn=="978123");assert(p.series=="Series");assert(p.seriesIndex && *p.seriesIndex==2.5f);
      assert(p.textReferenceHref=="ch.xhtml#start");assert(p.guideTocPageHref=="toc.xhtml");assert(p.cssFiles.size()==1);
      assert(cache.getSpineCount()==1);
    }
    assert(cache.endContentOpfPass());assert(cache.beginTocPass());
    cache.createTocEntry("Title","ch.xhtml","start",0);
    assert(cache.endTocPass());assert(cache.endWrite());
    zipEntrySizes["ch.xhtml"]=1234;
    BookMetadataCache::BookMetadata m{"Title","Author","en","","ch.xhtml#start"};
    assert(cache.buildBookBin("book.epub",m));assert(cache.load());
    assert(cache.getSpineEntry(0).href=="ch.xhtml");assert(cache.getSpineEntry(0).tocIndex==0);
    assert(cache.getCumulativeSize(0)==1234);assert(cache.getSpineCumulativeSize(0)==1234);
    assert(cache.getSpineIndexForSize(100)==0);assert(cache.getSpineIndexForSize(1235)==-1);
    assert(cache.getTocEntry(0).title=="Title");
  }
  Storage.reset();
  for(size_t count: {size_t(33),size_t(301)}) {
    BookMetadataCache c("/large");assert(c.beginWrite());assert(c.beginContentOpfPass());
    for(size_t i=0;i<count;++i) {auto h=std::to_string(i)+".xhtml";c.createSpineEntry(h);zipEntrySizes[h]=10;}
    assert(c.endContentOpfPass());assert(c.beginTocPass());c.createTocEntry("Last",std::to_string(count-1)+".xhtml","",0);
    assert(c.endTocPass());assert(c.endWrite());
    assert(c.buildBookBin("large.epub",{}));assert(c.load());
    for(size_t i=0;i<count;++i){assert(c.getCumulativeSize(i)==(i+1)*10);assert(c.getSpineIndexForSize(i*10+1)==int(i));}
    assert(c.getTocEntry(0).spineIndex==int(count-1));
  }
  {
    const std::string big="<package><metadata><title>"+std::string(65536,'A')+"</title></metadata></package>";
    ContentOpfParser p(empty,empty,big.size(),nullptr);feed(p,big,37);assert(p.title.size()==512);
  }
  {
    ContentOpfParser p(empty,empty,xml.size(),nullptr,false,false);feed(p,xml,17);assert(p.cssFiles.empty());assert(p.isbn=="978123");
  }
  {
    const std::string bad="<package><metadata></package>";ContentOpfParser p(empty,empty,bad.size(),nullptr);
    assert(p.setup());assert(p.write(reinterpret_cast<const uint8_t*>(bad.data()),bad.size())==0);
  }
  {
    // Only rebuildable metadata is rejected; durable sibling state stays untouched.
    Storage.put("/book/progress",{1,2,3});Storage.put("/book/bookmarks",{4,5});
    std::vector<uint8_t> old(32,0);const uint32_t magic=0x425843FF;
    std::memcpy(old.data(),&magic,sizeof(magic));old[4]=9;
    Storage.put("/book/book.bin",old);BookMetadataCache c("/book");assert(!c.load());
    assert((Storage.bytes("/book/progress")==std::vector<uint8_t>{1,2,3}));
    assert((Storage.bytes("/book/bookmarks")==std::vector<uint8_t>{4,5}));
  }
  {
    // Current unified TXT/Markdown facade uses raw byte size, not ZIP metadata.
    Storage.put("/raw.txt",std::vector<uint8_t>(42,'t'));
    BookMetadataCache c("/raw");assert(c.beginWrite());assert(c.beginContentOpfPass());c.createSpineEntry("/raw.txt");
    assert(c.endContentOpfPass());assert(c.beginTocPass());assert(c.endTocPass());assert(c.endWrite());
    assert(c.buildBookBin("/raw.txt",{}));assert(c.load());assert(c.getCumulativeSize(0)==42);
  }
  std::cout<<"PASS production OPF streamed/entity/identifier/series/CSS/guide + metadata cold/warm/windows/large-index/bounds/malformed/derived-rejection/durable-sentinels/raw-TXT\n";
}
