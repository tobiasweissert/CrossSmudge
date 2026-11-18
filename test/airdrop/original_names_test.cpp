// Production StorageSink and decoder over checked memory-only I/O.
#define main previous_storage_main
#include "storage_test.cpp"
#undef main

static void originalName() {
  MemoryFiles files; StorageSink sink(files);
  assert(sink.begin("My Book.epub", 3, token)); assert(write(sink, 3)); assert(sink.finish());
  assert(std::string(sink.savedPath()) == "/Inbox/My Book.epub");
  assert(files.files.at("/Inbox/My Book.epub") == std::vector<uint8_t>({'a','a','a'}));
  std::puts("PASS AD-NAME-001 original basename published without postfix");
}
static void originalRecovery() {
  for (const char* name : {"Résumé 你好 📖.EPUB", "MiXeD.tar.GZ", "README", "a.b.C", "e\xcc\x81.txt"}) {
    MemoryFiles files; StorageSink sink(files);
    assert(sink.begin(name, 3, token)); assert(write(sink, 3)); assert(sink.finish());
    const auto before = files.files;
    StorageSink recovered(files);
    assert(recovered.inspect("01020304050000000000000000000000") == State::PublishedVerified);
    assert(std::string(recovered.savedPath()) == std::string("/Inbox/") + name);
    assert(files.files == before);
  }
  std::puts("PASS AD-NAME-002 exact UTF8/case/extensions survive read-only journal recovery");
}
static void knownCollision() {
  for (bool directory : {false, true}) {
    MemoryFiles files; files.directories.insert("/Inbox");
    if (directory) files.directories.insert("/Inbox/book.txt");
    else files.files["/Inbox/book.txt"] = {'o','l','d'};
    files.files["/metadata/book.txt.progress"] = {'4','2'};
    const auto before = files.files; const auto dirs = files.directories;
    StorageSink sink(files);
    assert(!sink.begin("book.txt", 3, token));
    assert(std::string(sink.error()).find("collision") != std::string::npos);
    assert(files.files == before && files.directories == dirs && files.renames == 0);
  }
  std::puts("PASS AD-NAME-003 known file/directory collision refuses staging and preserves metadata");
}
// Independent journal fixtures: do not use StorageSink's serializer to build legacy data.
static MemoryFiles historical(uint8_t version, unsigned last) {
  MemoryFiles files;
  const std::string dir = "/Inbox/.airdrop/01020304050000000000000000000000";
  const std::string final = version == 1 ? "/Inbox/book (AirDrop 01020304050000000000000000000000).txt" : "/Inbox/book.txt";
  files.directories.insert("/Inbox"); files.directories.insert("/Inbox/.airdrop"); files.directories.insert(dir);
  const std::vector<uint8_t> bytes = {'a','a','a'};
  uint8_t digest[32]; SHA256(bytes.data(), bytes.size(), digest);
  for (unsigned phase = 1; phase <= last; ++phase) {
    std::vector<uint8_t> record(512);
    std::memcpy(record.data(), "ADSTG001", 8); record[8] = version; record[9] = phase; record[12] = 3;
    if (phase > 1) std::memcpy(record.data() + 16, digest, 32);
    std::strcpy(reinterpret_cast<char*>(record.data() + 64), "book.txt");
    std::strcpy(reinterpret_cast<char*>(record.data() + 185), final.c_str());
    std::memcpy(record.data() + 400, "01020304050000000000000000000000", 32);
    const auto crc = journalCrc32(record.data(), 508);
    for (unsigned i=0;i<4;++i) record[508+i] = uint8_t(crc >> (8*i));
    files.files[dir + "/phase." + std::to_string(phase)] = record;
  }
  files.files[last == 4 ? final : dir + "/payload.part"] = bytes;
  return files;
}
static void journalCompatibility() {
  const char* hex = "01020304050000000000000000000000";
  for (uint8_t version : {1,2}) for (unsigned last : {1,2,3,4}) {
    auto files = historical(version,last); const auto before = files.files;
    StorageSink sink(files);
    const State expected[] = {State::Idle, State::RetainedPartial, State::CompleteStaged, State::Uncertain, State::PublishedVerified};
    assert(sink.inspect(hex) == expected[last]);
    assert(files.files == before && files.renames == 0);
    assert(!sink.finish()); assert(files.files == before && files.renames == 0);
  }
  for (unsigned mode=0;mode<4;++mode) {
    auto files=historical(2,4);
    auto& record=files.files.at(std::string("/Inbox/.airdrop/")+hex+"/phase.2");
    if(mode==0) record[8]=1; // Same path bytes, different policy: forbidden.
    if(mode==1) record[8]=3;
    if(mode==2) std::strcpy(reinterpret_cast<char*>(record.data()+185), "/Inbox/other.txt");
    if(mode==3) {
      auto& first=files.files.at(std::string("/Inbox/.airdrop/")+hex+"/phase.1");
      std::strcpy(reinterpret_cast<char*>(first.data()+185), "/Inbox/../other.txt");
      const auto crc=journalCrc32(first.data(),508);
      for(unsigned i=0;i<4;++i) first[508+i]=uint8_t(crc>>(8*i));
    }
    const auto crc=journalCrc32(record.data(),508);
    for(unsigned i=0;i<4;++i) record[508+i]=uint8_t(crc>>(8*i));
    const auto before=files.files;StorageSink sink(files);
    assert(sink.inspect(hex)==State::Uncertain);assert(files.files==before&&files.renames==0);
  }
  for(uint8_t version:{1,2}) for(bool replace:{false,true}) {
    auto files=historical(version,4);
    const std::string path=version==1?"/Inbox/book (AirDrop 01020304050000000000000000000000).txt":"/Inbox/book.txt";
    files.files.erase(path);if(replace)files.files[path]={'n','e','w'};
    const auto before=files.files;StorageSink receipt(files);
    assert(receipt.inspectJournal(hex)==State::PublishedVerified);
    StorageSink payload(files);assert(payload.inspect(hex)==State::Uncertain);
    assert(files.files==before&&files.renames==0);
  }
  // Inspecting a legacy inactive partial must not make subsequent journals v1.
  {auto files=historical(1,1);const auto before=files.files;StorageSink sink(files);
    assert(sink.inspect(hex)==State::RetainedPartial);const uint8_t next[16]={9};
    assert(sink.begin("new.txt",3,next));assert(write(sink,3));assert(sink.finish());
    assert(std::string(sink.savedPath())=="/Inbox/new.txt");
    assert(files.files.at(std::string(sink.recoveryPath())+"/phase.1")[8]==2);
    for(const auto&[path,data]:before)assert(files.files.at(path)==data);}
  std::puts("PASS AD-NAME-005 independent v1/v2 phases, mixed/unknown versions, forged paths, terminal move/delete/replacement; no republish");
}
static void raceAndPolicy() {
  for(unsigned mode=0;mode<3;++mode) {
    MemoryFiles files;StorageSink sink(files);assert(sink.begin("book.txt",3,token));assert(write(sink,3));
    files.files["/metadata/book.txt.progress"]={'4','2'};
    if(mode==0)files.files["/Inbox/book.txt"]={'o','l','d'};
    if(mode==1)files.directories.insert("/Inbox/book.txt");
    if(mode==2)files.readError=true;
    const auto before=files.files;
    assert(!sink.finish());assert(sink.state()==State::Failed&&files.renames==0);
    for(const auto&[path,data]:before)assert(files.files.at(path)==data);
    assert(files.files.at(std::string(sink.recoveryPath())+"/payload.part")==std::vector<uint8_t>({'a','a','a'}));
    assert(files.files.count(std::string(sink.recoveryPath())+"/phase.2"));
  }
  for(const char* path:{"/Inbox","/Inbox/.airdrop"}) {
    MemoryFiles files;files.directories.insert("/Inbox");
    if(std::string(path)=="/Inbox")files.directories.erase(path);
    files.files[path]={'o','l','d'};const auto before=files.files;
    StorageSink sink(files);assert(!sink.begin("book.txt",3,token));assert(files.files==before&&files.renames==0);
  }
  for(const char* name:{"../book.txt","a/b.txt","a\\b.txt","CON.txt","nul","LPT1.PDF",".airdrop","bad .txt","bad."," bad.txt","bad.txt ","bad:txt"}) {
    MemoryFiles files;StorageSink sink(files);assert(!sink.begin(name,3,token));assert(files.files.empty()&&files.directories==std::set<std::string>{"/"});
  }
  std::string longest(116,'x');longest+=".TXT";
  {MemoryFiles files;StorageSink sink(files);assert(sink.begin(longest.c_str(),0,token));assert(sink.finish());assert(std::string(sink.savedPath())=="/Inbox/"+longest);}
  longest+='x';{MemoryFiles files;StorageSink sink(files);assert(!sink.begin(longest.c_str(),0,token));assert(files.files.empty());}
  {MemoryFiles files;StorageSink sink(files);assert(sink.begin("Résumé.EPUB",3,token));assert(write(sink,2));sink.cancel();const auto before=files.files;
    StorageSink recovered(files);assert(recovered.inspect("01020304050000000000000000000000")==State::RetainedPartial);assert(files.files==before&&files.renames==0);}
  std::puts("PASS AD-NAME-006 late file/directory/unreadable collision retains stage/metadata; fixed Inbox, reject-only unsafe names, 120-byte bound, cancel recovery");
}
static void archiveToExactName() {
  for(const char* name:{"Résumé 你好.EPUB","e\xcc\x81.PDF","MiXeD.tar.GZ","README"}) {
    MemoryFiles files;StorageSink sink(files);assert(sink.begin(name,3,token));
    airdrop::stream::Manifest manifest{};std::strcpy(manifest.name,name);
    std::snprintf(manifest.archivePath,sizeof manifest.archivePath,"./%s",name);manifest.size=3;
    auto decoder=std::make_unique<airdrop::stream::Decoder>();assert(decoder->reset(manifest,{&sink,streamAppend,streamCancel}));
    auto archive=odc(manifest.archivePath,3);archive.insert(archive.end(),{'a','b','c'});
    const auto trailer=odc("TRAILER!!!",0,0);archive.insert(archive.end(),trailer.begin(),trailer.end());
    const uint32_t word=uint32_t(archive.size())|0x80000000u;
    const uint8_t header[]={uint8_t(word>>24),uint8_t(word>>16),uint8_t(word>>8),uint8_t(word)};
    size_t used=0;assert(decoder->feed(header,4,used)==airdrop::stream::Status::Progress&&used==4);
    for(const uint8_t byte:archive){assert(decoder->feed(&byte,1,used)==airdrop::stream::Status::Progress&&used==1);}
    assert(decoder->finish()==airdrop::stream::Status::Complete);assert(sink.finish());
    assert(std::string(sink.savedPath())==std::string("/Inbox/")+name);assert(files.files.at(sink.savedPath())==std::vector<uint8_t>({'a','b','c'}));
  }
  std::puts("PASS AD-NAME-007 exact accepted UTF8 basename from real bytewise DVZip/CPIO decoder to verified storage");
}
int main(int argc, char** argv) {
  assert(argc == 2);
  if (std::string(argv[1]) == "original") originalName();
  else if (std::string(argv[1]) == "recovery") originalRecovery();
  else if (std::string(argv[1]) == "collision") knownCollision();
  else if (std::string(argv[1]) == "journals") journalCompatibility();
  else if (std::string(argv[1]) == "policy") raceAndPolicy();
  else if (std::string(argv[1]) == "archive") archiveToExactName();
  else return 2;
  return 0;
}
