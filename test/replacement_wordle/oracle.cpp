#include "WordleCore.h"
#include <cstdio>
#include <cstdint>
int main(){
 uint32_t state=42;
 for(int n=0;n<1000;n++){char answer[6]={},guess[6]={};for(int i=0;i<5;i++){state=state*1664525u+1013904223u;answer[i]='A'+((state>>16)%8);state=state*1664525u+1013904223u;guess[i]='A'+((state>>16)%8);}wordle::Mark marks[5];wordle::score(guess,answer,marks);printf("S,%s,%s,",guess,answer);for(auto m:marks)printf("%d",(int)m);printf("\n");}
 for(int n=0;n<250;n++){std::string results;for(int i=0;i<40;i++){state=state*1664525u+1013904223u;results.push_back((char)(((state>>16)%9)|((state&2)?128:0)));}int today=n%40;auto stats=wordle::statsFor(results,today);printf("T,%d,",today);for(unsigned char c:results)printf("%02x",c);printf(",%d,%d,%d,",stats.played,stats.won,stats.streak);for(int i=0;i<6;i++)printf("%s%d",i?"/":"",stats.wins[i]);printf("\n");}
 for(int d=-3000;d<=4000;d+=7){int y,m,day;wordle::dateOfDay(d,y,m,day);printf("D,%d,%d,%d,%d\n",d,y,m,day);}
}
