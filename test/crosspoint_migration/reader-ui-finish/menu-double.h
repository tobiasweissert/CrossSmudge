enum class StrId {STR_SELECT_CHAPTER,STR_READING_STATS,STR_TRACK_READING_STATS,STR_MARK_UNFINISHED,STR_MARK_FINISHED,STR_DELETE_BOOK_STATS,STR_DELETE_CACHE,STR_SEND_NEARBY_BOOK,STR_DISABLE_TOUCHSCREEN,STR_ON,STR_OFF};
struct Translation {const char* get(StrId id)const{return id==StrId::STR_ON?"On":id==StrId::STR_OFF?"Off":"Label";}}I18N;
namespace freeink::ui {struct ListItem {const char* label=nullptr;const char* value=nullptr;int16_t actionValue=0;};}
namespace fui = freeink::ui;
struct MenuResult {int action;};
class XtcReaderMenuActivity {
 public:
 __ACTIONS__;
 struct MenuItem {MenuAction action;StrId labelId;};
 static std::vector<MenuItem>buildMenuItems(bool,bool,bool);
 std::vector<MenuItem>items;std::array<freeink::ui::ListItem,8>listItems{};int selectedIndex=0,result=-1;bool statsEnabled=true,finished=false;
 void refreshListItems();void selectCurrent();void requestUpdate(){}void setResult(MenuResult r){result=r.action;}void finish(){finished=true;}
};
struct ReaderDispatch {
 GfxRenderer renderer;bool enabled=true,ack=true;int resumes=0,updates=0;uint32_t currentPage=0;
 struct Stats {bool isCompleted=false;}stats;
 struct Book {std::string getPath(){return "book";}};std::unique_ptr<Book>xtc=std::make_unique<Book>();
 bool isBookStatsTrackingEnabled()const{return enabled;}
 bool setBookStatsTrackingEnabled(bool e){if(!ack)return false;enabled=e;return true;}
 void resumeReadingStatsTimer(const char*){++resumes;}void requestUpdate(){++updates;}
 void openChapterSelection(){}void openReadingStats(){}void setBookCompleted(bool){}void deleteBookStats(){}void deleteBookCache(){}bool saveProgress(uint32_t){return true;}
 void onReaderMenuConfirm(int);
};
struct Manager {void goToNearbyBookSend(const std::string&,bool){}}activityManager;
int toasts=0;void drawToast(const GfxRenderer&,const char*){++toasts;}
