// Source oracle: compile against the read-only CrossPlay checkout, not a reimplementation.
#include "SolitaireCore.h"
#include <cstdio>
#include <cstdint>
static void state(const solitaire::Game& g) {
 printf("%d,%d,%d",g.moveCount(),g.won(),g.canAutoFinish());
 for(int p=0;p<13;p++){printf("|");auto& pile=g.pile(p);for(int i=0;i<pile.count;i++)printf("%s%u",i?",":"",pile.cards[i]);}
 printf("\n");
}
int main(){
 const uint32_t seeds[]={0,1,42,12345,0x7fffffffu,0xffffffffu};
 for(auto seed:seeds)for(int mode=0;mode<2;mode++){
 solitaire::Game g;g.deal(seed,mode);printf("D,%d,%d\t",(int32_t)seed,mode);state(g);
 uint32_t rng=seed?seed:1;
 for(int step=0;step<220;step++){
 rng=rng*1664525u+1013904223u;
 if(step%11==10&&g.canUndo()){g.undo();printf("U\t");}
 else {
 int from[1000],to[1000],count[1000],n=0;
 for(int f=0;f<13;f++)for(int t=0;t<13;t++)for(int c=1;c<=24;c++)if(g.canMove(f,t,c)){from[n]=f;to[n]=t;count[n++]=c;}
 if(n&&step%4){int k=rng%(uint32_t)n;g.move(from[k],to[k],count[k]);printf("M,%d,%d,%d\t",from[k],to[k],count[k]);}
 else{g.drawFromStock();printf("S\t");}
 }
 state(g);
 }
 }
}
