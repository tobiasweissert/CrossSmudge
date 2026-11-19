#include "TtfRenderProfileStore.h"
#include <HalStorage.h>
#include <cassert>
int main(int argc,char**argv){assert(argc==2);testRoot=argv[1];
 TtfRenderProfileStore a;TtfRenderProfile p;p.twoBit=false;p.cacheKiB=64;p.maxGlyphs=512;
 assert(a.setProfile("Bitter",p));auto key=p.identity();assert(a.saveToFile());
 TtfRenderProfileStore b;assert(b.loadFromFile());assert(b.profileFor("Bitter")==p);
 assert(b.profileFor("Other")!=p);p.cacheKiB=0;assert(!b.setProfile("Bitter",p));
 assert(b.profileFor("Bitter").identity()==key);assert(b.resetProfile("Bitter"));assert(b.saveToFile());
 TtfRenderProfileStore c;assert(c.loadFromFile());assert(c.profileFor("Bitter")==TtfRenderProfile{});
 assert(!c.setProfile("../Bad",{}));
}
