#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
struct Rect { int x,y,width,height; };
struct GfxRenderer {
  int width=480, screenHeight=800;
  mutable std::vector<std::string> labels;
  mutable std::vector<Rect> fills;
  int getScreenWidth() const { return width; }
  int getScreenHeight() const { return screenHeight; }
  int getLineHeight(int) const { return 14; }
  void getOrientedViewableTRBL(int* t,int* r,int* b,int* l) const {*t=3;*r=5;*b=7;*l=11;}
  void fillRect(int x,int y,int w,int h,bool) const { assert(w>=0&&h>=0&&x>=0&&y>=0&&x+w<=width&&y+h<=screenHeight);fills.push_back({x,y,w,h}); }
  void drawRect(int x,int y,int w,int h,bool c) const {fillRect(x,y,w,h,c);}
  std::string truncatedText(int,const char* v,int width) const { return std::string(v).substr(0,static_cast<size_t>(width/4)); }
  void drawText(int,int x,int y,const char* v,bool) const { assert(x>=0&&x<=width&&y>=0&&y<screenHeight);labels.emplace_back(v); }
};
struct Clock { bool available=true; bool isAvailable() const {return available;} bool formatTime(char* b,size_t n,int,bool) {snprintf(b,n,"12:34");return true;} } halClock;
struct Power { int getBatteryPercentage() const {return 73;} } powerManager;
bool formatHeaderDateText(char* b,size_t n) { if(!halClock.available)return false;snprintf(b,n,"Jan 01");return true; }
constexpr int SMALL_FONT_ID=1;
