#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#define LOG_ERR(...) ((void)0)
#define LOG_INF(...) ((void)0)
std::map<std::string,std::string> disk;
std::vector<std::string> events;
std::string fail; bool after=false; int dirty=0;
bool ack(const std::string&s){events.push_back(s);return s!=fail;}
struct HalFile {std::string path;size_t pos=0;explicit operator bool()const{return !path.empty();}bool isDirectory(){return disk[path]=="DIR";}bool close(){return ack("close:"+path);}bool sync(){return ack("sync");}size_t size(){return disk[path].size();}int read(void*p,size_t n){if(!ack("read"))return -1;auto s=disk[path].substr(pos,n);memcpy(p,s.data(),s.size());pos+=s.size();return s.size();}size_t write(const uint8_t*p,size_t n){if(!ack("write"))return 0;disk[path].append(reinterpret_cast<const char*>(p),n);return n;}};
struct {bool exists(const char*p){return disk.count(p);}HalFile open(const char*p,int mode=0){if(mode){if(disk.count(p))return {};disk[p]="";}if(!disk.count(p))return {};return {p};}bool openFileForRead(const char*,const char*p,HalFile&f){f=open(p);return bool(f);}bool mkdir(const char*){return true;}bool remove(const char*p){if(!ack("remove:"+std::string(p)))return false;return disk.erase(p);}bool rename(const char*a,const char*b){bool ok=ack("rename:"+std::string(a));if((!ok&&!after)||!disk.count(a)||disk.count(b))return false;std::vector<std::pair<std::string,std::string>> changes;for(const auto&[p,v]:disk)if(p==a||p.rfind(std::string(a)+"/",0)==0)changes.push_back({p,v});for(const auto&[p,v]:changes){disk[std::string(b)+p.substr(strlen(a))]=v;if(ok)disk.erase(p);}return ok;}} Storage;
namespace FsHelpers {bool ext(const std::string&p,const char*x){return p.size()>=strlen(x)&&p.substr(p.size()-strlen(x))==x;}bool hasEpubExtension(const std::string&p){return ext(p,".epub");}bool hasXtcExtension(const std::string&p){return ext(p,".xtc");}bool hasTxtExtension(const std::string&p){return ext(p,".txt");}bool hasMarkdownExtension(const std::string&p){return ext(p,".md");}bool isSafePathComponent(const std::string&s){return !s.empty()&&s.find_first_of("/\\\n\r")==std::string::npos;}}
// FILE_PATHS
struct Epub {std::string p;Epub(const std::string&p,const char*):p(p){}static std::string cachePathForFilePath(const std::string&p,const char*){return "/cache/epub"+p;}std::string getCachePath(){return cachePathForFilePath(p,"");}bool clearCache(){events.push_back("clear-cache");return ack("clear");}};
struct Xtc {std::string p;Xtc(const std::string&p,const char*):p(p){}std::string getCachePath(){return "/cache/other"+p;}bool clearCache(){return ack("clear");}};using Txt=Xtc;
struct {std::string openEpubPath,favoriteSleepImagePath,favoriteBootImagePath;bool saveToFile(){return ack("state-save");}} APP_STATE;
struct RecentBook {std::string path,title,author;};
struct {std::vector<RecentBook> books;const auto&getBooks(){return books;}bool updatePath(const std::string&a,const std::string&b,const std::string&,const std::string&){if(!ack("recent"))return false;for(auto&v:books)if(v.path==a)v.path=b;return true;}void removeByPath(const std::string&){events.push_back("recent-remove");}bool saveToFile(){return ack("recent-save");}} RECENT_BOOKS;
namespace library {void markLibraryIndexDirty(){++dirty;}}
struct BookmarkStore {struct RenameMigration {bool uncertain=false;};static void deleteForFilePath(const std::string&,const char*){events.push_back("bookmark-delete");}static bool beginRenameMigration(const std::string&,const std::string&,const std::string&,const std::string&,const char*,RenameMigration&,bool){return ack("bookmark-prepare");}static bool commitRenameMigration(RenameMigration&){return ack("bookmark-commit");}static bool rollbackRenameMigration(RenameMigration&){return ack("bookmark-rollback");}};
struct ClippingStore {using RenameMigration=BookmarkStore::RenameMigration;bool loadForBook(const std::string&,const std::string&,const std::string&,const char*){return ack("clipping-validate");}static bool deleteForFilePath(const std::string&,const char*){return ack("clipping-delete");}static bool beginRenameMigration(const std::string&,const std::string&,const std::string&,const std::string&,const char*,RenameMigration&,bool){return ack("clipping-prepare");}static bool commitRenameMigration(RenameMigration&){return ack("clipping-commit");}static bool rollbackRenameMigration(RenameMigration&){return ack("clipping-rollback");}};
bool clearBookCachePreservingUserState(const std::string&){return ack("clear-preserving");}
// PRODUCTION
void reset(){disk.clear();events.clear();fail.clear();after=false;dirty=0;APP_STATE={};RECENT_BOOKS.books={{"/a.epub","title","author"}};disk["/a.epub"]="BOOK";disk["/cache/epub/a.epub"]="DIR";for(auto n:{"progress.bin","stats_v5.bin","reading_stats_off","reading_stats_off.tmp"})disk[std::string("/cache/epub/a.epub/")+n]=n;}
int main(){using namespace BookMoveUtils;reset();APP_STATE.openEpubPath="/a.epub";assert(moveFile("/a.epub","/b.epub")==RenameMigrationResult::Success);assert(!disk.count("/a.epub")&&disk["/b.epub"]=="BOOK"&&APP_STATE.openEpubPath=="/b.epub");for(auto n:{"progress.bin","stats_v5.bin","reading_stats_off","reading_stats_off.tmp"})assert(disk[std::string("/cache/epub/b.epub/")+n]==n);assert(!disk.count("/.crosspoint/book-move.pending")&&dirty==1);assert(std::count(events.begin(),events.end(),"rename:/a.epub")==1);
 for(auto fault:{"write","sync","read","close:/.crosspoint/book-move.pending","bookmark-prepare","clipping-prepare","rename:/cache/epub/a.epub","rename:/a.epub","recent","state-save","bookmark-commit","clipping-commit","remove:/.crosspoint/book-move.pending"}){reset();APP_STATE.openEpubPath="/a.epub";fail=fault;auto v=moveFile("/a.epub","/b.epub");assert(v==RenameMigrationResult::Uncertain);assert(disk.count("/.crosspoint/book-move.pending"));assert(std::find(events.begin(),events.end(),"bookmark-rollback")==events.end());auto before=disk;fail.clear();assert(moveFile("/a.epub","/c.epub")==RenameMigrationResult::Uncertain&&before==disk);}
 for(bool mutation:{false,true}){reset();fail="rename:/a.epub";after=mutation;assert(moveFile("/a.epub","/b.epub")==RenameMigrationResult::Uncertain);assert(disk.count("/a.epub"));assert(disk.count("/.crosspoint/book-move.pending"));}
 reset();disk["/b.epub"]="OTHER";auto before=disk;assert(moveFile("/a.epub","/b.epub")==RenameMigrationResult::RolledBack&&before==disk);
 reset();disk["/.crosspoint/clippings/move.pending"]="CPMOVE1";before=disk;assert(moveFile("/a.epub","/b.epub")==RenameMigrationResult::Uncertain&&before==disk);
 reset();fail="clipping-validate";before=disk;assert(moveFile("/a.epub","/b.epub")==RenameMigrationResult::RolledBack&&before==disk);
 reset();assert(!BookActions::clearBookCache("/a.epub"));assert(disk.count("/cache/epub/a.epub/reading_stats_off.tmp"));
 reset();fail="remove:/a.epub";assert(!BookActions::deleteFile("/a.epub"));assert(std::find(events.begin(),events.end(),"bookmark-delete")==events.end());
 reset();fail="clipping-delete";assert(!BookActions::deleteFile("/a.epub"));assert(!disk.count("/a.epub")&&dirty==1);
 reset();disk["/a.txt"]="TEXT";disk["/cache/other/a.txt"]="DIR";disk["/cache/other/a.txt/reading_stats_off.tmp"]="pending";assert(moveFile("/a.txt","/b.md")==RenameMigrationResult::Success);assert(disk["/cache/other/b.md/reading_stats_off.tmp"]=="pending");
 reset();disk["/a.txt"]="TEXT";disk["/cache/other/a.txt"]="DIR";disk["/cache/epub/a.txt"]="DIR";before=disk;assert(moveFile("/a.txt","/b.txt")==RenameMigrationResult::RolledBack&&before==disk);
 reset();disk["/a.txt"]="TEXT";disk["/cache/epub/a.txt"]="DIR";disk["/cache/epub/a.txt/progress.bin"]="position";assert(moveFile("/a.txt","/b.txt")==RenameMigrationResult::Success);assert(disk["/cache/epub/b.txt/progress.bin"]=="position");
 reset();assert(moveFile("/a.epub","/b.txt")==RenameMigrationResult::RolledBack);
 reset();assert(BookActions::deleteFile("/a.epub"));
 std::cout<<"PASS actual mutation orchestration: cache+progress+statsOff temp, single physical rename, checked intent, 13 phase faults, reboot retry fence, negative FAT acknowledgements, collision/retained-root refusal, delete failure propagation\n";
}
