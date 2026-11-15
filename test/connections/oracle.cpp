#include "ConnectionsCore.h"
#include <cstdio>
#include <cstdlib>
int main() {
  connections::Puzzle p{};
  for(int t=0;t<16;++t)snprintf(p.groups[t/4].members[t%4],33,"%d",t+1);
  connections::Game g;g.start(p,123);
  for(int i=0;i<16;++i)printf("%s%s",i?",":"",g.tileWord(i));puts("");
  for(unsigned mask:{23u,23u,15u,240u,3840u,61440u}) {
    g.deselectAll();for(int i=0;i<g.tileCount();++i)if(mask&(1u<<(atoi(g.tileWord(i))-1)))g.toggleTile(i);
    auto r=g.submit();printf("%d,%d,%d,%d\n",int(r),g.mistakes(),g.solvedCount(),int(g.result()));
  }
}
