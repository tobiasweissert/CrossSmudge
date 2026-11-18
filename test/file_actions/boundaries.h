#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>
#include <cstring>
#include <limits>
#define LOG_ERR(...) ((void)0)
#define LOG_INF(...) ((void)0)
void delay(int){}
bool failActivityAllocation = false;
template <typename T, typename... Args>
std::unique_ptr<T> makeUniqueNoThrow(Args&&... args) {
  if (failActivityAllocation) return {};
  return std::unique_ptr<T>(new (std::nothrow) T(std::forward<Args>(args)...));
}
const char* tr(StrId id){switch(id){case StrId::STR_FILES_MEMORY_FAILED:return "Not enough memory to open file actions.";case StrId::STR_FILES_UNSAFE:return "This file cannot be changed safely.";case StrId::STR_FILES_MISSING:return "File no longer available. Nothing was deleted.";case StrId::STR_FILES_DELETE_FAILED:return "Delete failed. File metadata and reading progress were retained.";case StrId::STR_FILES_CACHE_CLEANUP_FAILED:return "File deleted, but its reading cache could not be fully removed.";case StrId::STR_FILES_STATE_CLEANUP_FAILED:return "File deleted, but app-state cleanup could not be saved. Inspect the SD card before retrying.";case StrId::STR_FILES_MOVE_UNCERTAIN:return "Move uncertain. Stop using the SD card and inspect both paths on a computer before retrying.";case StrId::STR_FILES_MOVE:return "Move...";case StrId::STR_FILES_MOVE_TO_FOLDER:return "Move to folder";case StrId::STR_FILES_DESTINATION_EXISTS:return "Destination already exists. Nothing was moved.";case StrId::STR_FILES_MOVE_FAILED:return "Move failed. The original file was retained.";case StrId::STR_FILES_FAVORITES_SAVE_FAILED:return "File moved, but favorite paths could not be saved. Inspect the SD card before retrying.";case StrId::STR_FILES_TITLE:return "Files";case StrId::STR_FILES_SELECT_FOLDER:return "Select folder";case StrId::STR_FILES_ACTIONS:return "Actions (hold Open)";default:return "localized";}}
[[maybe_unused]] struct I18nAdapter { const char* get(StrId){return "localized";} } I18N;
struct FilePathResult {std::string path;};
struct FileBrowserActionResult {int action;};
struct KeyboardResult {std::string text;};
struct OptionSelectionResult {uint8_t index;};
struct ActivityResult {
 std::variant<std::monostate,FilePathResult,FileBrowserActionResult,KeyboardResult,OptionSelectionResult> data;
 bool isCancelled=false;
 ActivityResult()=default;
 template<class T> ActivityResult(T x):data(std::move(x)){}
};
std::vector<std::string> events;
struct GfxRenderer {int w=480,h=800;int getScreenWidth()const{return w;}int getScreenHeight()const{return h;}int getTextWidth(int,const char*s)const{return int(strlen(s))*8;}int getLineHeight(int)const{return 20;}void drawText(int,int,int,const char*){}void clearScreen(){}void displayBuffer(){}std::string truncatedText(int,const char*s,int){return s;}};
struct MappedInputManager {
 enum class Button{Back,Confirm,Left,Right,Up,Down,Power};
 enum class SwipeDir{None,Up,Down,Left,Right};
 std::set<Button> pressed,released,held;
 bool down=false,tap=false,contact=false,touchReleased=false;int x=0,y=0;unsigned long heldTime=0;
 bool wasPressed(Button b)const{return pressed.count(b);}
 bool wasReleased(Button b)const{return released.count(b);}
 bool isPressed(Button b)const{return held.count(b);}
 bool isPhysicalPressed(Button b)const{return held.count(b);}
 bool wasAnyPressed()const{return !pressed.empty();}
 bool wasAnyReleased()const{return !released.empty();}
 bool isScreenContactActive()const{return contact;}
 bool wasScreenTouchDown(int& a,int& b)const{a=x;b=y;return down;}
 bool wasScreenTapped(int& a,int& b)const{a=x;b=y;return tap;}
 bool wasScreenTouchReleased()const{return touchReleased;}
 bool isScreenTouchHeld(int& a,int& b)const{a=x;b=y;return contact;}
 bool hasTouchHardware()const{return true;}
 struct Labels{const char*btn1;const char*btn2;const char*btn3;const char*btn4;};
 const char* withBackArrow(const char*s)const{return s;}
 Labels mapLabels(const char*a,const char*b,const char*c,const char*d)const{return {a,b,c,d};}
 bool hasTouch()const{return true;}
 unsigned long getHeldTime()const{return heldTime;}
 SwipeDir wasSwipe()const{return SwipeDir::None;}
 void suppressCurrentTouchContact(){tap=false;}
 void frame(){pressed.clear();released.clear();down=tap=touchReleased=false;}
};
struct Activity {
 struct RenderLock{template<class T> explicit RenderLock(T&) {}};
 virtual ~Activity()=default;
 virtual void onEnter(){} virtual void loop(){} virtual void render(RenderLock&&){}
 ActivityResult result;bool finished=false;
 void setResult(ActivityResult r){result=std::move(r);}void finish(){finished=true;}
};
struct FileBrowserActionActivity:Activity {
 struct MenuItem{FileBrowserAction action;StrId labelId;const char* label=nullptr;};
 std::vector<MenuItem> items;
 FileBrowserActionActivity(GfxRenderer&,MappedInputManager&,std::string,std::vector<MenuItem> i,bool=false,bool=true):items(std::move(i)){}
};
struct ConfirmationActivity: Activity {
 template<class... T> ConfirmationActivity(T&&...){}
};
struct OptionSelectionActivity:Activity{template<class...T>OptionSelectionActivity(T&&...){}};
using FileMutationConfirmationActivity=ConfirmationActivity;
struct KeyboardEntryActivity:Activity{template<class...T>KeyboardEntryActivity(T&&...){}};
struct SettingsActivity:Activity{enum class View{FileBrowser};template<class...T>SettingsActivity(T&&...){}};
struct RecentBook {std::string path,title,author;};
struct AppStateAdapter {
 char pendingAlertTitle[64]{},pendingAlertBody[512]{};
 std::atomic<bool> hasPendingAlert{false},pendingAlertGoHomeOnBack{false};
 std::string openEpubPath="/Books/Original.epub",favoriteSleepImagePath,favoriteBootImagePath;
 bool saveOk=true;bool saveToFile(){events.push_back("save-open");return saveOk;}
} APP_STATE;
struct RecentAdapter {
 std::vector<RecentBook> books;bool updateOk=true;
 bool removeByPath(const std::string&){events.push_back("recent");return true;}
 const std::vector<RecentBook>& getBooks()const{return books;}
 bool updatePath(const std::string&,const std::string&,const std::string&,const std::string&){events.push_back("recent-move");return updateOk;}
} RECENT_BOOKS;
struct FileHandle {
 bool valid=false,dir=false;
 explicit operator bool()const{return valid;}
 bool isDirectory()const{return dir;}
 void close(){}
};
struct StorageAdapter {
 bool removeOk=true;int renameFailure=0;int renames=0;int renameFailureEffect=0;
 std::map<std::string,bool> paths;
 std::vector<std::pair<std::string,std::string>> moves;
 bool remove(const char* p){events.push_back("remove");if(!removeOk)return false;paths.erase(p);return true;}
 FileHandle open(const char*p)const{auto i=paths.find(p);return {i!=paths.end(),i!=paths.end()&&i->second};}
 bool exists(const char*p)const{return paths.count(p);}
 bool rename(const char*a,const char*b){
   events.push_back("rename");moves.push_back({a,b});++renames;
   if(renames==renameFailure){if(renameFailureEffect&&paths.count(a)){paths[b]=paths[a];if(renameFailureEffect==2)paths.erase(a);}return false;}
   if(!paths.count(a)||paths.count(b))return false;
   paths[b]=paths[a];paths.erase(a);return true;
 }
} Storage;
namespace FsHelpers {
 bool ends(const std::string&p,const char*e){return p.size()>=strlen(e)&&p.substr(p.size()-strlen(e))==e;}
 bool hasEpubExtension(const std::string&p){return ends(p,".epub");}
 bool hasXtcExtension(const std::string&p){return ends(p,".xtc");}
 bool hasTxtExtension(const std::string&p){return ends(p,".txt");}
 bool hasMarkdownExtension(const std::string&p){return ends(p,".md");}
 bool hasPngExtension(const std::string&p){return ends(p,".png");}
 bool hasBmpExtension(const std::string&p){return ends(p,".bmp");}
 std::string extractFolderPath(const std::string&p){auto n=p.rfind('/');return n==0?"/":p.substr(0,n);}
 bool isSafePathComponent(const std::string&p){return !p.empty()&&p!="."&&p!=".."&&p.find_first_of("/\\:")==std::string::npos;}
}
struct Epub {
 static inline bool cacheOk=true;
 std::string path;Epub(const std::string&p,const char*):path(p){}
 static std::string cachePathForFilePath(const std::string&p,const char*){return "/cache"+p;}
 std::string getCachePath()const{return "/cache"+path;}
 bool clearCache()const{events.push_back("cache");if(!cacheOk)return false;Storage.paths.erase(getCachePath());return true;}
};
using Xtc=Epub;using Txt=Epub;
namespace BookActions {
 bool clearFileMetadata(const std::string&);
 std::vector<FileBrowserActionActivity::MenuItem> buildBookActionItems(const std::string&,bool){return {{FileBrowserAction::Delete,StrId::STR_DELETE}};}
 bool canSendNearby(const std::string&){return true;}
 bool clearBookCache(const std::string&){return true;}
 bool deleteBookStats(const std::string&){return true;}
 bool resetBookReaderSettings(const std::string&){return true;}
 std::string confirmationHeading(StrId){return "confirm";}
 void drawToast(const GfxRenderer&,const char* s){events.push_back(std::string("notice:")+s);}
 bool toggleBookCompleted(const std::string&,const std::string&,bool&){return true;}
 uint8_t epubRenderModeDisplayIndex(uint8_t x){return x;}
 uint8_t epubRenderModeForDisplayIndex(uint8_t x){return x;}
 std::vector<std::string> epubRenderModeOptions(){return {};}
}
namespace EpubReaderActivity {
 uint8_t loadBookRenderMode(const std::string&){return 0;}
 bool saveBookRenderMode(const std::string&,uint8_t){return true;}
}
namespace ImageFolderIndex {void invalidateForPath(const char*){events.push_back("image-index");}}
struct {void markRegistryDirtyForPath(const char*){events.push_back("font-index");}} sdFontSystem;
struct ListNav {
 int selected=0,top=0,visibleRows=1;
 template<class T,class U>void syncToProps(T,int,int,int,U&){}
 void reset(int s){selected=s;}
 bool scrollBy(int,int){return false;}int pageRowsFor(int){return 1;}void follow(int){}
 bool consumeRebuildNeeded(){return false;}
};
int followListSelection(int,int,int,int){return 0;}
int scrollListBy(int,int,int,int){return 0;}
std::string getFileName(std::string p){return p;}
std::string getFileExtension(const std::string&p){auto n=p.rfind('.');return n==std::string::npos?"":p.substr(n);}
std::string utf8ComposeNfc(const std::string&p){return p;}
std::string buildFullPath(std::string p,const std::string&e){return p+(p.back()=='/'?"":"/")+e;}
std::string normalizeDirectoryPath(std::string p){while(p.size()>1&&p.back()=='/')p.pop_back();return p;}
bool isSleepImageFile(const std::string&p){return FsHelpers::hasPngExtension(p);}
bool isBootImageFile(const std::string&p){return FsHelpers::hasBmpExtension(p);}
bool isDefaultSleepFolderPath(const std::string&){return false;}
#include "ui_boundaries.h"
// INPUT_GATE
struct FileBrowserActivity:Activity {
 FileMutationInputGate pickerInputGate;
 FileMutationInputGate mutationListInputGate;
 std::atomic<bool> mutationListFence{false};
 enum class Mode{Books,PickFirmware,PickDirectory};Mode mode=Mode::Books;
 GfxRenderer renderer;MappedInputManager mappedInput;
 std::function<void(const ActivityResult&)> callback;
 std::unique_ptr<Activity> child;
 size_t selectorIndex=2;int updates=0;std::string basepath="/Books",directoryPickerTitle;
 std::vector<std::string> files{"A.epub","Original.epub"};
 bool longPressConfirmHandled=false,pendingCompletedFeedback=false,completedFeedbackIsFinished=false;
 unsigned long completedFeedbackShowTime=0;
 bool showFileSelection=true,lockNextConfirmRelease=false,lockLongPressBack=false,longPressBackHandled=false;
 bool uiReady=false;size_t actionWindowFirst=0;
 bool fileListMemoryLimited=false,fileListReadFailed=false;
 struct App{
  std::map<int,std::pair<void(*)(const fui::ActionEvent&,void*),void*>>handlers;
  std::vector<fui::Screen::Button> buttons;int pending=0;
  void on(int a,void(*f)(const fui::ActionEvent&,void*),void*u){handlers[a]={f,u};}
  void clearTapFlash(){}void render(){}bool invalidated(){return false;}
  bool route(fui::InputSnapshot s){
   for(const auto&b:buttons)if(s.x>=b.rect.x&&s.x<b.rect.x+b.rect.width&&s.y>=b.rect.y&&s.y<b.rect.y+b.rect.height){
    if(s.touchPressed)pending=b.props.action;
    if(s.touchReleased&&pending==b.props.action&&handlers.count(pending)){auto h=handlers[pending];pending=0;h.first({},h.second);return true;}
   }
   return false;
  }
 }app;
 int uiTarget=0;
 ButtonNavigator buttonNavigator;
 int topIndex=0,visibleRows=5;ListNav listNav;
 struct{void goToNearbyBookSend(const std::string&,bool){}}activityManager;
 FileBrowserActivity()=default;
 FileBrowserActivity(GfxRenderer&r,MappedInputManager&i,std::string p,Mode m):mode(m),renderer(r),mappedInput(i),basepath(std::move(p)){}
 void startActivityForResult(std::unique_ptr<Activity>a,std::function<void(const ActivityResult&)> cb){child=std::move(a);callback=std::move(cb);}
 void promptDeleteFile(const std::string&,const std::string&);
 void buildListScreen(UiApp::ScreenType&);
 void loop()override;void render(RenderLock&&)override;void navigateBack();
 static void onRowEvent(const fui::ActionEvent&,void*);
 static void onSettingsEvent(const fui::ActionEvent&,void*){}
 static void onFileControlEvent(const fui::ActionEvent&,void*);
 void selectCurrentFolder();
 void registerActions();
 void activateSelected();
 std::string openedPath;
 void onSelectBook(const std::string&p){openedPath=p;}
 void onGoHome(){finished=true;}
 void toggleHiddenFiles(){}
 bool isPreferredSleepFolder(const std::string&)const{return false;}
 void fileOperationError(const char*);
 void showFileActionMenu(const std::string&,bool=false);
 void startMoveFile(const std::string&,const std::string&);
 void moveFile(const std::string&,const std::string&,const std::string&);
 void renameFile(const std::string&,const std::string&,const std::string&,const std::string&);
 void startRenameFile(const std::string&,const std::string&){}
 void showDirectoryActionMenu(const std::string&,bool=false);
 void promptDeleteDirectory(const std::string&,const std::string&,bool=false){}
 void setPreferredSleepFolder(const std::string&){}void clearPreferredSleepFolder(){}
 bool isPinnedSleepFavorite(const std::string&p)const{return APP_STATE.favoriteSleepImagePath==p;}
 bool isPinnedBootFavorite(const std::string&p)const{return APP_STATE.favoriteBootImagePath==p;}
 void unpinSleepFavorite(){APP_STATE.favoriteSleepImagePath.clear();}
 void unpinBootFavorite(){APP_STATE.favoriteBootImagePath.clear();}
 void pinSleepFavorite(const std::string&){}void pinBootFavorite(const std::string&){}
 void loadFilesLocked(){events.push_back("refresh");files.clear();for(const auto&p:Storage.paths)if(FsHelpers::extractFolderPath(p.first)==basepath&&p.first!=basepath)files.push_back(p.first.substr(p.first.rfind('/')+1)+(p.second?"/":""));}
 size_t entryCount()const{return files.size();}
 const char* entryNameAt(size_t i){return files.at(i).c_str();}
 size_t findEntry(const std::string&p){auto i=std::find(files.begin(),files.end(),p);return i==files.end()?0:size_t(i-files.begin());}
 void requestUpdate(bool=true){++updates;}
};
unsigned long millis(){return 42;}
constexpr size_t NAME_BUFFER_SIZE=500;
constexpr size_t MAX_VIRTUAL_LIST_ENTRIES=32767;
constexpr fui::ActionId ACTION_ROW=1,ACTION_SETTINGS=2;
constexpr fui::ActionId ACTION_FILE_CONTROL=3;
constexpr unsigned long GO_HOME_MS=1000,COMPLETED_FEEDBACK_MS=1000;
constexpr int ROOT_HINT_GAP=20;
