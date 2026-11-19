#pragma once
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <cstring>
#include <cstdint>
struct EpdGlyph{int width=8,height=16,left=0,top=16;};
struct EpdFontFamily{enum Style{REGULAR=0,BOLD=1,SUP=2,SUB=4};const EpdGlyph* getGlyph(uint32_t,Style)const{static EpdGlyph g;return &g;}};
enum class Color{Black,White,LightGray,DarkGray,Clear};
class GfxRenderer{
public:
 enum class Orientation{Portrait,Landscape,PortraitInverted,LandscapeInverted};
 int getScreenWidth()const{return 480;}int getScreenHeight()const{return 800;}int getLineHeight(int)const{return 20;}int getFontAscenderSize(int)const{return 16;}
 Orientation getOrientation()const{return Orientation::Portrait;}
 void setOrientation(Orientation){}
 void getOrientedViewableTRBL(int*t,int*r,int*b,int*l)const{*t=*r=*b=*l=0;}
 int getTextWidth(int,const char*s,EpdFontFamily::Style=EpdFontFamily::REGULAR)const{return std::strlen(s)*8;}
 std::string truncatedText(int,const char*s,int,EpdFontFamily::Style=EpdFontFamily::REGULAR)const{return s;}
 std::vector<std::string> wrappedText(int,const char*s,int,int,EpdFontFamily::Style)const{return {s};}
 const std::map<int,EpdFontFamily>& getFontMap()const{static std::map<int,EpdFontFamily> m;return m;}
#define DRAW(name) template<class...A>void name(A&&...)const{}
 template<class...T>bool drawBitmap(T&&...)const{return true;}
 DRAW(setClipRect) DRAW(drawLine) DRAW(drawPixel) DRAW(drawRect) DRAW(drawRoundedRect) DRAW(drawText) DRAW(drawTextDither) DRAW(drawTextRotated90CW) DRAW(drawTextRotated90CWDither) DRAW(fillPolygon) DRAW(fillRect) DRAW(fillRectDither) DRAW(fillRoundedRect) DRAW(clearScreen) DRAW(displayBuffer) DRAW(drawCenteredText) DRAW(maskRoundedRectOutsideCorners)
#undef DRAW
};
