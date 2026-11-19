from activity_bodies import method

def emit(root,out):
    t=(root/'src/activities/reader/ReaderActivity.cpp').read_text()
    code='''#include <atomic>
#include <cassert>
#include <cstdio>
#include <string>
#include "ReaderSession.h"
struct RecentBook {enum class CoverState {Missing,Unknown};};
struct RecentStore {int adds=0;void addOrUpdateBook(const std::string&,const std::string&,const std::string&,const std::string&,RecentBook::CoverState){++adds;}} RECENT_BOOKS;
struct State {std::string openEpubPath;void saveToFile(){}} APP_STATE;
namespace pluginevents {struct Var {const char*key;const char*value;};enum class Event {ReaderOpen};inline int opens=0;void emit(Event,const Var*,int){++opens;}}
static uint32_t now=1000;
unsigned long millis(){return now;}
namespace trustedtime { int64_t trustedNow(){return 1800000000;} }
class ReaderActivity {public:
 ReaderSession readerSession;
 std::atomic<int8_t> pendingSessionTurn{0};
 std::atomic<bool> pageRendered{false};int progress=0;
 bool bookRemembered=false,recentUpdateOnRender=true;std::string bookPath="/book";
 std::string getBookTitle()const{return "Book";}
 std::string getBookAuthor()const{return "Author";}
 std::string getBookThumbBmpPath()const{return "";}
 void rememberBookOnceRendered();
 void markPageRendered(){pageRendered=true;}
 int getProgressBasisPoints()const{return progress;}
 void notePageTurn(bool,bool);void noteRenderedPage();
};
struct RenderLock { explicit RenderLock(ReaderActivity&) {assert(false && "nested owner render lock");} };
'''+method(t,'ReaderActivity::notePageTurn')+'\n'+method(t,'ReaderActivity::noteRenderedPage')+'\n'+method(t,'ReaderActivity::rememberBookOnceRendered')+'''
int main(){
 ReaderActivity r;r.rememberBookOnceRendered();assert(RECENT_BOOKS.adds==0&&pluginevents::opens==0);
 r.noteRenderedPage();assert(r.pageRendered);
 r.rememberBookOnceRendered();r.rememberBookOnceRendered();assert(RECENT_BOOKS.adds==1&&pluginevents::opens==1);
 ReaderActivity fast;fast.recentUpdateOnRender=false;fast.noteRenderedPage();fast.rememberBookOnceRendered();
 assert(RECENT_BOOKS.adds==1&&pluginevents::opens==2);
 now=16000;r.notePageTurn(true,true);r.progress=100;r.noteRenderedPage();
 assert(r.readerSession.durationSeconds()==15);
 now=26000;r.notePageTurn(false,true);r.noteRenderedPage();assert(r.readerSession.durationSeconds()==15);
 now=41000;r.notePageTurn(true,true);r.progress=200;r.noteRenderedPage();assert(r.readerSession.durationSeconds()==30);
 r.noteRenderedPage();assert(r.readerSession.durationSeconds()==30);
 puts("PASS actual common reader plugin-session handoff under owner render lock, forward dwell, back/no-double-render");
}
'''
    (out/'activities.cpp').write_text(code)
