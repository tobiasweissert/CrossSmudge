// Synthetic HAL/codec/renderer orchestration adapter. NOT rendered-pixel evidence.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define PNG_PIXEL_GRAYSCALE 0
#define PNG_PIXEL_TRUECOLOR 2
#define PNG_PIXEL_INDEXED 3
#define PNG_PIXEL_GRAY_ALPHA 4
#define PNG_PIXEL_TRUECOLOR_ALPHA 6
constexpr bool TURN_OFF_SCREEN_AFTER_SLEEP_REFRESH=true;
struct CrossPointSettings { enum SLEEP_SCREEN_MODE {DARK,LIGHT,CUSTOM,COVER,BLANK,COVER_CUSTOM,QUICK_RESUME,OVERLAY,READING_STATS_SLEEP,MINIMAL_SLEEP,MINIMAL_STATS_SLEEP,DASHBOARD_SLEEP,TRANSPARENT_CUSTOM,COVER_TRANSPARENT}; enum SLEEP_SCREEN_COVER_MODE{FIT,CROP}; enum SLEEP_SCREEN_COVER_FILTER{NO_FILTER,BLACK_AND_WHITE,INVERTED_BLACK_AND_WHITE};};
struct {int sleepScreen=12,sleepScreenCoverMode=0,sleepScreenCoverFilter=0;} SETTINGS;
struct CrossPointState {static constexpr int SLEEP_AVOID_REPEAT_LOOKBACK=8;};
struct {std::string openEpubPath; int saves=0;void getSleepOverlayLCG(uint32_t&a,uint32_t&b,uint16_t&c,uint16_t&d){a=b=c=d=0;}void getSleepLCG(uint32_t&a,uint32_t&b,uint16_t&c,uint16_t&d){a=b=c=d=0;}void setSleepOverlayLCG(uint32_t,uint32_t,uint16_t,uint16_t){}void setSleepLCG(uint32_t,uint32_t,uint16_t,uint16_t){}void saveToFile(){++saves;}} APP_STATE;
long random(long n){return n?1%n:0;}
bool oom=false;
template<class T> std::unique_ptr<T> makeUniqueNoThrow(size_t n){return oom?nullptr:std::unique_ptr<T>(new(std::nothrow) typename std::remove_extent<T>::type[n]);}
struct Node {std::vector<uint8_t> data;std::vector<std::string> children;bool dir=false;};
std::map<std::string,Node> files;
struct HalFile {std::string path;size_t pos=0,index=0;HalFile()=default;explicit HalFile(std::string p):path(std::move(p)){}explicit operator bool()const{return !path.empty()&&files.count(path);}bool isDirectory()const{return bool(*this)&&files.at(path).dir;}void close(){path.clear();}bool seek(size_t p){pos=p;return bool(*this)&&p<=files[path].data.size();}bool seekCur(size_t n){return seek(pos+n);}int read(){return bool(*this)&&pos<files[path].data.size()?files[path].data[pos++]:-1;}int read(uint8_t*p,size_t n){size_t c=bool(*this)&&pos<=files[path].data.size()?std::min(n,files[path].data.size()-pos):0;if(c)memcpy(p,files[path].data.data()+pos,c);pos+=c;return c;}HalFile openNextFile(){if(!isDirectory()||index>=files[path].children.size())return {};return HalFile(files[path].children[index++]);}void getName(char*p,size_t n){auto name=path.substr(path.find_last_of('/')+1);snprintf(p,n,"%s",name.c_str());}};
struct {HalFile open(const char*p){return files.count(p)?HalFile(p):HalFile();}bool exists(const char*p){return files.count(p);}bool openFileForRead(const char*,const std::string&p,HalFile&f){f=open(p.c_str());return bool(f);}} Storage;
namespace FsHelpers {bool hasBmpExtension(std::string_view p){return p.size()>=4&&p.substr(p.size()-4)==".bmp";}bool hasPngExtension(std::string_view p){return p.size()>=4&&p.substr(p.size()-4)==".png";}}
enum class BmpReaderError {Ok,Error};
struct Bitmap {HalFile& file; bool gray=true;int rewinds=0;Bitmap(HalFile&f,bool=false,bool=false):file(f){}BmpReaderError parseHeaders(){return bool(file)&&files[file.path].data.size()>=54?BmpReaderError::Ok:BmpReaderError::Error;}int getWidth()const{return 4;}int getHeight()const{return 1;}bool hasGreyscale()const{return gray;}BmpReaderError rewindToData(){++rewinds;return file.seek(54)?BmpReaderError::Ok:BmpReaderError::Error;}static const char*errorToString(BmpReaderError){return "synthetic";}};
struct HalDisplay {enum RefreshMode{HALF_REFRESH};enum class GrayscaleMode{Direct,Absolute};};
struct GfxRenderer {
 enum RenderMode {BW,GRAYSCALE_LSB,GRAYSCALE_MSB};mutable RenderMode mode=BW;bool absolute=true,direct=true,storeOk=true,baseOk=true,drawOk=true;mutable int frame=7,saved=0,stores=0,restores=0,discard=0,bases=0,lsb=0,msb=0,gray=0,bw=0,draws=0,pixels=0,inversions=0; mutable bool poweredOff=false,borrowed=false;
 int getScreenWidth()const{return 4;}int getScreenHeight()const{return 4;}
 struct Caps{bool yes;bool supported()const{return yes;}};Caps grayscaleCapabilities(HalDisplay::GrayscaleMode m)const{return {m==HalDisplay::GrayscaleMode::Direct?direct:absolute};}
 bool supportsAbsoluteGrayscale()const{return absolute;} bool supportsDirectGrayscale()const{return direct;}
 bool grayPlanesAreAbsolute()const{return absolute&&bases;}
 bool storeBwBuffer(){++stores;if(!storeOk)return false;saved=frame;return true;}void restoreBwBuffer(bool=true){++restores;frame=saved;}void discardStoredBwBuffer(){++discard;}
 void clearScreen(int n=255)const{assert(!borrowed);frame=n;}void invertScreen()const{assert(!borrowed);++inversions;frame=~frame;}
 bool drawBitmap(const Bitmap&,int,int,int,int,float=0,float=0,bool=false)const{assert(!borrowed);++draws;frame=55;return drawOk;}
 mutable std::vector<std::pair<int,bool>> writes;
 void drawPixel(int x,int y,bool black)const{assert(!borrowed);++pixels;writes.push_back({y*4+x,black});}
 void displayBuffer(HalDisplay::RefreshMode,bool off=false)const{assert(!borrowed);++bw;poweredOff=off;}
 bool displayGrayscaleBase(HalDisplay::GrayscaleMode)const{assert(!borrowed);++bases;return baseOk;}
 void displayGrayscaleBase(HalDisplay::RefreshMode)const{assert(!borrowed);++bases;}
 void copyGrayscaleLsbBuffers()const{++lsb;}void copyGrayscaleMsbBuffers()const{++msb;}void displayGrayBuffer(bool off=false)const{++gray;poweredOff=off;}
 void setRenderMode(RenderMode m){mode=m;}
};
// The runner inserts the actual GrayPlanePixel/grayPlanePixel production body here.
/* GRAY_PLANE_PRODUCTION */
struct ImageDimensions{int width=4,height=1;};
struct RenderConfig{int x=0,y=0,maxWidth=0,maxHeight=0;bool useDithering=true,useExactDimensions=false,preserveAlpha=false;float sourceCropX=0,sourceCropY=0;};
int decodeCalls=0,failDecode=0;bool dimsOk=true;
struct PngToFramebufferConverter {static bool getDimensionsStatic(const std::string&,ImageDimensions&){return dimsOk;}bool decodeToFramebuffer(const std::string&,GfxRenderer&r,const RenderConfig&c){assert(c.preserveAlpha&&!c.useDithering);assert(!r.borrowed);++decodeCalls;if(failDecode==decodeCalls){r.frame=123;return false;}return true;}};
namespace SleepCoverAssets {inline bool cached=false,prepareOk=true;std::string cachedCoverPathFor(const std::string&,bool,bool){return cached?"/cover.bmp":"";}bool prepareFullCoverForPath(const std::string&,bool,GfxRenderer*r,bool){r->borrowed=true;r->frame=999;r->borrowed=false;r->frame=255;cached=prepareOk;return prepareOk;}}
struct SleepActivity {GfxRenderer&renderer;std::string currentBookPath;mutable Bitmap*overlayCover=nullptr,*overlayGrayCover=nullptr;mutable int defaults=0;explicit SleepActivity(GfxRenderer&r):renderer(r){}void renderDefaultSleepScreen()const{++defaults;renderer.clearScreen();renderer.displayBuffer(HalDisplay::HALF_REFRESH,true);}void renderCustomSleepScreen()const{renderDefaultSleepScreen();}bool renderBitmapSleepScreen(Bitmap&)const{return true;}void renderCoverSleepScreen()const;void renderModernBitmapSleepScreen(Bitmap&,bool=false)const;bool renderSleepOverlayFile(HalFile&,const char*)const;bool renderTransparentOverlayPng(const std::string&)const;bool renderSleepOverlayPath(const std::string&)const;void renderTransparentCustomSleepScreen()const;};
