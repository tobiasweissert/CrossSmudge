#!/usr/bin/env python3
"""Production-extracted reader regression slices; no full Activity/SD/UI emulation.
Each slice is copied verbatim from the selected source, not a parallel fix model.
"""
import argparse, fcntl, hashlib, json, os, subprocess
from pathlib import Path
R = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--case', type=int, choices=range(1, 6), required=True)
p.add_argument('--source', type=Path, help='Read-only alternate production cpp for baseline replay')
p.add_argument('--sanitize', action='store_true')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
source=a.source or R/'src/activities/reader/EpubReaderActivity.cpp'
s = source.read_text()
print('SOURCE',str(source),'SHA256',hashlib.sha256(s.encode()).hexdigest(),flush=True)
def between(start, end):
    return s[s.index(start):s.index(end, s.index(start))]
def method(name):
    start = s.index('void EpubReaderActivity::'+name+'(')
    end = s.index('\n}\n', start)+3
    return s[start:end].replace('EpubReaderActivity::', '')
base = r'''
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <iostream>
#include "ManualPageTurnQueue.h"
#define LOG_ERR(...) ((void)0)
#define LOG_DBG(...) ((void)0)
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(0)
unsigned long clockMs=10000; unsigned long millis() { return clockMs; }
struct Section { int currentPage=0,pageCount=10; bool isBuilding(){return false;} bool isPartial(){return false;} void abandonBuild(){} void clearCache(){} };
// Checked synthetic pointer turns a forbidden null dereference into a bounded assertion,
// not a crash test. Successful accesses are ordinary host Section accesses.
struct Slot { std::unique_ptr<Section> p; explicit operator bool()const{return bool(p);} Section* operator->(){if(!p)throw std::runtime_error("null section dereference");return p.get();} void reset(){p.reset();} void build(int page=0){p=std::make_unique<Section>();p->currentPage=page;} };
struct Epub { int getSpineItemsCount(){return 2;} std::string getCachePath(){return "cache";} std::string getPath(){return "book";} int getSpineIndexForTextReference(){return 1;} size_t getBookSize(){return 200;} bool resolveLocationPercentToSpineProgress(float,int& i,float& v){i=1;v=.5;return true;} size_t getCumulativeSpineItemSize(int i){return (i+1)*100;} int resolveHrefToSpineIndex(const std::string&){return 1;} bool isNavigationDocumentSpine(int,bool* ok){*ok=true;return false;} };
struct End { bool active=false;bool menuActive(){return active;} };
struct RenderLock { template<class T> RenderLock(T&){} static bool peek(){return false;} };
struct GfxRenderer { struct FrameBufferLoan { FrameBufferLoan(int){} }; };
struct BookReadingStats { int totalPagesTurned=0,totalReadingSeconds=100,pace=0;bool isCompleted=false,startDateManual=false,finishedDateManual=false;int startDate=0,finishedDate=0;static BookReadingStats disk;static BookReadingStats load(std::string){return disk;} };
BookReadingStats BookReadingStats::disk;
struct GlobalReadingStats { int totalPagesTurned=0,totalReadingSeconds=100,pace=0,completedBooks=0;static GlobalReadingStats disk;static GlobalReadingStats load(){return disk;} };
GlobalReadingStats GlobalReadingStats::disk;
struct Settings {bool moveFinishedToReadFolder=true;} SETTINGS;
struct State {uint16_t pendingBookmarkSpine=UINT16_MAX,pendingBookmarkParagraphIndex=UINT16_MAX,pendingClippingIndex=UINT16_MAX;float pendingBookmarkProgress=-1;void saveToFile(){} } APP_STATE;
namespace EpubReaderUtils { struct Progress {int spineIndex=0,pageNumber=4,pageCount=10;bool hasPageCount=true,hasVisibleTextOffset=true;uint32_t visibleTextOffset=123;};bool hasProgress=false; bool loadProgress(Epub&,Progress&){return hasProgress;} }
float clampPercent(float x){return std::max(0.f,std::min(x,100.f));}
struct Harness {
 std::unique_ptr<Epub> epub=std::make_unique<Epub>();Slot section;std::unique_ptr<End>endOfBookOptions;
 int currentSpineIndex=0,nextPageNumber=0,pageLoadRetryCount=0;std::optional<uint16_t>pendingPageJump;
 unsigned long lastPageTurnTime=0;static constexpr unsigned long MIN_MANUAL_PAGE_TURN_GAP_MS=200;
 ManualPageTurnQueue pendingManualPageTurns;QueuedTurnRenderingState queuedTurnRendering;
 bool activeFootnotePreview=false,completionPromptQueued=false,completionPromptShown=false,pendingReadFolderMove=false;
 bool pendingPercentJump=false,pendingRelayoutReposition=false;float pendingSpineProgress=0;
 int cachedSpineIndex=0,cachedChapterPageNumber=0,cachedChapterTotalPageCount=0;std::optional<uint32_t>cachedVisibleTextOffset;
 uint16_t pendingParagraphIndex=UINT16_MAX,pendingClippingIndex=UINT16_MAX;
 std::string pendingAnchor,pendingFootnotePreviewAnchor;int renderer=0;
 struct Pos{int spineIndex=0,pageNumber=0;};static constexpr int MAX_FOOTNOTE_DEPTH=4;Pos savedPositions[4];int footnoteDepth=0;
 BookReadingStats stats;GlobalReadingStats globalStats;int sessionReadingSeconds=60,updates=0,homes=0;
 void requestUpdate(){++updates;}void onGoHome(){++homes;}
 void finishManualPageTurnBrakeIfReady(){} void clearPendingManualPageTurns(){pendingManualPageTurns.clear();}
 bool forwardPageReadElapsed(uint32_t& v,const char*){v=30;return true;}
 void recordCurrentPageReadingTime(const char*){}void armReadingPaceWarmup(const char*){}
 void recordForwardPagePaceSample(uint32_t,const char*){++stats.pace;++globalStats.pace;}
 bool shouldQueueCompletionPromptOnChapterExit(){return false;}
 void pauseReadingPaceTimer(const char*){}void resumeReadingPaceTimer(const char*){}void openReaderMenu(){}
 bool isInReadFolder(std::string){return false;}
 bool isTocChapterDestination(int,const std::string&)const{return false;}
 bool shouldUseFootnotePreview(int i,const std::string&)const{return i!=currentSpineIndex;}
'''
if a.case in (1, 2, 4):
    base += '\nvoid restoreSavedPosition(){}\n'
parts = []
tests = ''
if a.case == 1:
    parts = [method('requestManualPageTurn'), method('pageTurn')]
    # The two unchanged production switch prefixes contain the exact PAGE_TURN/PREVIOUS_PAGE routes.
    for signature in ['const uint8_t rawAction', 'const CrossPointSettings::SHORT_PWRBTN action']:
        start = s.index('bool EpubReaderActivity::handleShortcutAction('+signature+')')
        end = s.index('    case CrossPointSettings::SHORT_PWRBTN::NEARBY_POSITION_SYNC:', start)
        parts.append(s[start:end].replace('EpubReaderActivity::','')+'    default: return false;\n  }\n}\n')
    base = '#include <cstdint>\nnamespace CrossPointSettings { enum class SHORT_PWRBTN:uint8_t {IGNORE,PAGE_TURN,PREVIOUS_PAGE}; }\n'+base
    tests = r'''
for(bool typed:{false,true})for(bool forward:{true,false}){
 Harness h;h.section.build(9);h.currentSpineIndex=1;
 h.pageTurn(true,"button"); // actual last-page transition creates null-section sentinel
 CHECK(!h.section && h.currentSpineIndex==2);clockMs+=1000;
 auto action=forward?CrossPointSettings::SHORT_PWRBTN::PAGE_TURN:CrossPointSettings::SHORT_PWRBTN::PREVIOUS_PAGE;
 CHECK(typed?h.handleShortcutAction(action):h.handleShortcutAction(static_cast<uint8_t>(action)));
 if(forward)CHECK(h.homes==1);else CHECK(h.currentSpineIndex==1 && h.pendingPageJump==UINT16_MAX);
}
Harness missing;missing.requestManualPageTurn(true,"shortcut");missing.requestManualPageTurn(false,"shortcut");CHECK(missing.homes==0);
Harness menu;menu.currentSpineIndex=2;menu.endOfBookOptions=std::make_unique<End>();menu.endOfBookOptions->active=true;menu.requestManualPageTurn(false,"shortcut");CHECK(menu.currentSpineIndex==2);
'''
elif a.case == 2:
    parts = [method(n) for n in ['pageTurn','applyBookStatsEditsFromDisk','handleBookStatsReturn','onFrontlightPanelOpened','onFrontlightPanelClosed']]
    commit=between('    const uint32_t elapsedSecs = sessionReadingSeconds;', '      if (hasSessionStartLocalDateTime)')
    parts+=['void commitSession(){'+commit+'}}']
    base=base.replace('int totalPagesTurned=0,totalReadingSeconds=100,pace=0;', 'int totalPagesTurned=0,totalReadingSeconds=100,pace=0,sessionCount=0;')
    base=base.replace('int totalPagesTurned=0,totalReadingSeconds=100,pace=0,completedBooks=0;', 'int totalPagesTurned=0,totalReadingSeconds=100,pace=0,completedBooks=0,totalSessions=0;')
    tests = r'''
for(bool edit:{false,true}){
 Harness h;h.section.build();h.pageTurn(true,"button");h.pageTurn(true,"button");
 auto turns=h.stats.totalPagesTurned,pace=h.stats.pace;h.onFrontlightPanelOpened();
 if(edit){ // same preview-copy contract used by the actual drawer child
  BookReadingStats::disk=h.stats;BookReadingStats::disk.totalReadingSeconds+=h.sessionReadingSeconds;
  BookReadingStats::disk.isCompleted=true;BookReadingStats::disk.startDate=42;
  GlobalReadingStats::disk=h.globalStats;GlobalReadingStats::disk.completedBooks=1;
 }
 h.onFrontlightPanelClosed();
 CHECK(h.stats.totalPagesTurned==turns && h.globalStats.totalPagesTurned==turns);
 CHECK(h.stats.pace==pace && h.globalStats.pace==pace);
 h.commitSession();
 CHECK(h.stats.totalReadingSeconds==160 && h.stats.sessionCount==1);
 CHECK(h.globalStats.totalReadingSeconds==160 && h.globalStats.totalSessions==1);
 if(edit)CHECK(h.stats.startDate==42 && h.completionPromptShown && h.pendingReadFolderMove);
}
'''
elif a.case == 3:
    # Full production navigation methods; callback success bodies are exact fragments.
    parts = [method(n) for n in ['clearFootnotePreviewState','navigateToHref','restoreSavedPosition','jumpToPercent','jumpToStablePage','handleClippingJump']]
    if 'void EpubReaderActivity::abandonFootnoteNavigation(' in s:
        parts.insert(1, method('abandonFootnoteNavigation'))
    parts += ['void chapterJump(){ struct {int spineIndex=1;std::string anchor;} chapterResult;'+between('              clearFootnotePreviewState();' if '              clearFootnotePreviewState();' in s else '              abandonFootnoteNavigation();','            } else if (returnToReaderMenu)')+'}']
    begin=s.index('const auto& bm = std::get<BookmarkResult>')
    start=s.index('              ',s.index('RenderLock lock(*this);',begin)+len('RenderLock lock(*this);'))
    end=s.index('              pauseReadingPaceTimer("bookmark_jump");',start)+len('              pauseReadingPaceTimer("bookmark_jump");')
    parts += ['void bookmarkJump(){struct{int spineIndex=1;float progress=.7;uint16_t paragraphIndex=UINT16_MAX;}bm;'+s[start:end]+'}']
    base=base.replace('int resolveHrefToSpineIndex', 'bool resolveReferencePageTarget(uint32_t,int& i,float& p,uint32_t& o,uint32_t& c,bool& chars){i=1;p=.5;o=50;c=100;chars=false;return true;} int resolveHrefToSpineIndex')
    base=base.replace('bool isBuilding(){', 'int estimatedTotalPages(){return pageCount;} bool isBuilding(){')
    base=base.replace('struct Harness {','struct ClippingJumpResult{int spineIndex=1;uint16_t page=6,paragraphIndex=UINT16_MAX,clippingIndex=UINT16_MAX;};\nstruct Harness {')
    base+='''
 std::optional<uint32_t>pendingReferenceUnitOffset;uint32_t pendingReferenceUnitCount=0;bool pendingReferenceUnitsAreCharacters=false;std::optional<uint16_t>pendingResolvedReferencePage;
 uint16_t pageFromStoredProgress(float p,uint16_t count){return p*count;}
 int resolveParagraphJumpPage(Section&,uint16_t,uint16_t p){return p;}
'''
    parts+=['void exitProgress(){'+between('  if (footnoteDepth == 0 && !flushQueuedProgress())', '\n\n  // Reset orientation')+between('  if (footnoteDepth > 0 && epub)', '\n\n  BOOKMARKS.unload();')+'}']
    base+='''
 int persistedSpine=-1,persistedPage=-1;
 bool flushQueuedProgress(){persistedSpine=currentSpineIndex;persistedPage=section->currentPage;return true;}
 bool saveFootnoteOriginProgress(){persistedSpine=savedPositions[0].spineIndex;persistedPage=savedPositions[0].pageNumber;return true;}
'''
    tests = r'''
for(bool preview:{false,true})for(int route=0;route<5;++route){
 Harness h;h.section.build(3);h.navigateToHref(preview?"notes#note":"#note",true,false);
 CHECK(h.footnoteDepth==1 && h.savedPositions[0].pageNumber==3);
 h.section.build(0); // controlled section-builder boundary, not a forced origin stack
 const bool buildingFootnotePreview=!h.pendingFootnotePreviewAnchor.empty();
 h.activeFootnotePreview=buildingFootnotePreview;
 if(route==0)h.jumpToPercent(75);if(route==1)h.jumpToStablePage(20);
 if(route==2)h.handleClippingJump(ClippingJumpResult{});if(route==3)h.chapterJump();if(route==4)h.bookmarkJump();
 CHECK(h.footnoteDepth==0 && !h.activeFootnotePreview && h.pendingFootnotePreviewAnchor.empty());
 int target=h.currentSpineIndex;h.restoreSavedPosition();CHECK(h.currentSpineIndex==target);
 h.section.build(6);h.exitProgress();CHECK(h.persistedSpine==target && h.persistedPage==6);
}
Harness nested;nested.section.build(3);nested.navigateToHref("#a",true,false);nested.section.build(5);nested.navigateToHref("#b",true,false);nested.restoreSavedPosition();CHECK(nested.footnoteDepth==1 && nested.nextPageNumber==5);nested.restoreSavedPosition();CHECK(nested.footnoteDepth==0 && nested.nextPageNumber==3);
'''
elif a.case == 4:
    parts=[method('pageTurn')]
    retry=between('      pageLoadRetryCount++;','\n      LOG_ERR("ERS", "Failed to load page from SD after')
    parts+=['void failedRead(){'+retry+'}']
    # Exact default reload branch and pending-page assignment; SD builder is synthetic.
    default=between('      section->currentPage = nextPageNumber;','\n    }\n\n    if (!pendingAnchor.empty())')
    parts+=['void reload(){section.build();if(pendingPageJump){section->currentPage = *pendingPageJump;pendingPageJump.reset();}else{'+default+'}}']
    base+='\nbool automaticPageTurnActive=false;static constexpr int MAX_PAGE_LOAD_RETRIES=2;void showPendingSyncSaveError(){}\n'
    tests=r'''
for(int entry:{0,3}){
 Harness h;h.nextPageNumber=entry;h.reload();h.pageTurn(true,"button");h.pageTurn(true,"button");int target=h.section->currentPage;
 h.failedRead();CHECK(!h.section);h.reload();CHECK(h.section->currentPage==target);
 h.failedRead();h.reload();CHECK(h.section->currentPage==target);
 h.failedRead();CHECK(h.section && h.pageLoadRetryCount==3 && h.section->currentPage==target);
}
Harness jumped;jumped.nextPageNumber=0;jumped.pendingPageJump=7;jumped.reload();jumped.failedRead();jumped.reload();CHECK(jumped.section->currentPage==7);
Harness newer;newer.reload();newer.pageTurn(true,"button");newer.failedRead();newer.pendingPageJump=8;newer.reload();CHECK(newer.section->currentPage==8);
'''
elif a.case == 5:
    u=(R/'test/epub_reader_regressions/baseline/EpubReaderUtils.h').read_text()
    assert hashlib.sha256(u.encode()).hexdigest()=='bb73b5a6a119c5cb7cc2755b47c6f0ef350dda655928495856d3e3ac9481d926'
    lo=u.index('inline bool loadProgress(');hi=u.index('\n}\n',lo)+3
    base=base.replace('std::string getCachePath(){', 'std::string getCachePath()const{')
    base=base.replace('bool hasProgress=false; bool loadProgress(Epub&,Progress&){return hasProgress;}', 'int progressMode=0;bool readProgressFile(const char*,const std::string& path,Progress&){return progressMode==1 || (progressMode==2 && path.find(".bak")!=std::string::npos);}\n'+u[lo:hi])
    start='  bool loadedProgress = false;' if '  bool loadedProgress = false;' in s else '  if (APP_STATE.pendingBookmarkSpine != UINT16_MAX'
    parts=['void enter(){'+between(start,'\n  if (resumePageBuildAfterRestart)')+'}']
    tests=r'''
EpubReaderUtils::progressMode=0;Harness fresh;fresh.enter();CHECK(fresh.currentSpineIndex==1);
for(int mode:{1,2}){EpubReaderUtils::progressMode=mode;Harness resume;resume.enter();CHECK(resume.currentSpineIndex==0 && resume.nextPageNumber==4 && resume.cachedVisibleTextOffset==123 && resume.pendingRelayoutReposition);}
APP_STATE.pendingBookmarkSpine=0;APP_STATE.pendingBookmarkProgress=.6;Harness bookmark;bookmark.enter();CHECK(bookmark.currentSpineIndex==0 && bookmark.pendingPercentJump);
'''
else:
    raise SystemExit('case not authored yet')
if a.case == 3:
    base=base.replace('void reset(){p.reset();}', 'Section& operator*(){return *operator->();} void reset(){p.reset();}')
    tests=tests.replace(';if(route', ';\n if(route')
code = base+'\n'.join(parts)+'\n};\nint main(){try{'+tests+'\nstd::cout<<"PASS CC-N1-00'+str(a.case)+'\\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL CC-N1-00'+str(a.case)+': "<<e.what()<<"\\n";return 1;}}\n'
cpp=a.output/'slice.cpp';exe=a.output/'slice';cpp.write_text(code)
lockpath=Path('/root/.hermes/cache/scratch/crosscub-review-probes.lock')
with lockpath.open('a') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-I'+str(R/'src/activities/reader'),str(cpp),'-o',str(exe)]
    if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    print('COMPILE',json.dumps(cmd),flush=True)
    built=subprocess.run(cmd,timeout=45)
    if built.returncode:raise SystemExit(built.returncode)
    run=subprocess.run([str(exe)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'},timeout=10)
    print('TEST_EXIT',run.returncode,flush=True)
    if sum(p.stat().st_size for p in a.output.rglob('*') if p.is_file())>10*1024*1024:raise SystemExit('output bound exceeded')
    exe.unlink()
    raise SystemExit(run.returncode)
