#include "ChatLayout.h"
#include <cassert>
#include <cstdio>
int main() {
 for(int h=0;h<=1200;++h) {
  auto l=stickychat::chatLayout(h);
  assert(l.bubble>=0 && l.button>=0 && l.status>=0);
  assert(2*l.bubble+3*l.button+5*l.gap+l.status<=h);
 }
 for(int h:{400,720,400,720}) { auto l=stickychat::chatLayout(h); assert(l.button>=44 && l.bubble>=44); }
 puts("conversation vertical geometry bounds/all oriented bands: PASS");
}
