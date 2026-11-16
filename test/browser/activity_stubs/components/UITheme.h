#pragma once
#include <GfxRenderer.h>
class UITheme {
 public:
 struct Metrics {int topPadding=1,headerHeight=1;};
 static UITheme& getInstance(){static UITheme t;return t;}
 Metrics getMetrics(){return {};}
 void drawHeader(GfxRenderer&,Rect,const char*,const char*){}
 void drawButtonHints(GfxRenderer&,const char*,const char*,const char*,const char*){}
};
#define GUI UITheme::getInstance()
