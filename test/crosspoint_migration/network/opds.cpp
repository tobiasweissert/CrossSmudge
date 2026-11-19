#include <Opds2Parser.h>
#include <cassert>
#include <string>
#include <iostream>
int main(){
 const std::string feed=R"({"metadata":{"title":{"fr":"Livres","en":"B\u00fccher \ud83d\udcd6"},"numberOfItems":12,"itemsPerPage":5,"currentPage":2},"navigation":[{"title":"Browse","href":"/browse","type":"application/opds+json"}],"publications":[{"metadata":{"title":"A title","author":[{"name":"First"},{"name":"Second"}]},"links":[{"rel":"http://opds-spec.org/acquisition/open-access","type":"application/epub+zip","href":"/book.epub"}]}],"links":[{"rel":"next","href":"/next"}]})";
 for(size_t chunk:{size_t(1),size_t(13),feed.size()}){
  Opds2Parser parser;parser.setPreferredLanguage("en");
  for(size_t i=0;i<feed.size();i+=chunk)parser.write(reinterpret_cast<const uint8_t*>(feed.data()+i),std::min(chunk,feed.size()-i));
  parser.flush();assert(!parser.error());assert(parser.getFeedTitle()=="B\xc3\xbc" "cher \xf0\x9f\x93\x96");
  assert(parser.currentPage()==2 && parser.pageCount()==3 && parser.getNextPageUrl()=="/next");
  const auto& entries=parser.getEntries();assert(entries.size()==2);
  assert(entries[0].type==OpdsEntryType::NAVIGATION && entries[0].href=="/browse");
  assert(entries[1].type==OpdsEntryType::BOOK && entries[1].href=="/book.epub");
 }
 Opds2Parser partial;partial.write(reinterpret_cast<const uint8_t*>(feed.data()),feed.size()-1);partial.flush();assert(partial.error());
 std::cout<<"selected SDK production OPDS2: chunked Unicode, language, navigation, acquisition, pagination, incomplete rejection PASS\n";
}
