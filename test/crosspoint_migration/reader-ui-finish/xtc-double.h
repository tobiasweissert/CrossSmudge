namespace xtc {struct ChapterInfo {uint32_t startPage=0,endPage=0;char name[64]={};};}
struct Xtc {
  bool chapters=true;
  uint32_t getPageCount() const {return 100;}
  std::string getTitle() const {return "Book";}
  bool hasChapters() const {return chapters;}
  bool getChapterForPage(uint32_t p,xtc::ChapterInfo& c) const {if(!chapters)return false;c.startPage=(p/10)*10;c.endPage=c.startPage+9;std::strcpy(c.name,"Chapter");return true;}
};
constexpr int STR_UNNAMED=1,STR_TIME_LEFT_CALCULATING=2;
const char* tr(int x){return x==STR_UNNAMED?"Unnamed":"Calculating";}
constexpr int MIN_TIME_LEFT_PACE_SAMPLE_COUNT=3;
void formatCompactReadingDuration(uint32_t s,char* b,size_t n){snprintf(b,n,"%us",s);}
struct XtcReaderActivity {
  enum class StatusBarOverlayPosition {Bottom,Top};
  struct StatusBarInfo {int currentPage,pageCount;std::string title;};
  std::unique_ptr<Xtc> xtc=std::make_unique<Xtc>();
  GfxRenderer renderer;
  struct Stats {uint32_t avgSecondsPerForwardPage=10,paceSampleCount=4;}stats;
  bool statusBarVisible=true;
  StatusBarInfo getStatusBarInfo(uint32_t) const;
  bool formatTimeLeftLabel(char*,size_t,uint32_t,uint8_t=255) const;
  void renderStatusBarOverlay(StatusBarOverlayPosition,uint32_t) const;
};
