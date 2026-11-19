int main(){
 for(bool bookAck:{false,true})for(bool globalAck:{false,true}){
  BookStatsActivity a;a.didChangeStats=true;a.stats.candidate=42;Stats::ack=bookAck;Global::ack=globalAck;
  a.beginDateEditing();a.finishDateEditing(true);
  if(bookAck&&globalAck){assert(!a.didChangeStats&&a.page==BookStatsActivity::Page::PerBook&&a.refreshed==1);}
  else {
   assert(a.didChangeStats&&a.page==BookStatsActivity::Page::EditDates&&a.dateEditSnapshotValid&&a.renderer.displayed==1);
   a.exitStatsActivity();assert(!a.finished&&a.didChangeStats);
   a.finishDateEditing(false);assert(a.didChangeStats&&a.stats.candidate==42);
   Stats::ack=Global::ack=true;a.exitStatsActivity();assert(a.finished&&!a.didChangeStats&&a.stats.candidate==42);
  }
 }
 BookStatsActivity a;a.beginDateEditing();a.stats.candidate=17;a.didChangeStats=true;a.finishDateEditing(false);assert(!a.didChangeStats&&a.stats.candidate==0);
 a.applyCompletedState(true);assert(a.globalStats.completedBooks==1&&a.stats.finishedDate.day==7);a.applyCompletedState(true);assert(a.globalStats.completedBooks==1);a.applyCompletedState(false);assert(a.globalStats.completedBooks==0);
 a.editable=false;a.didChangeStats=true;int writes=Stats::writes;a.saveStats();assert(!a.didChangeStats&&Stats::writes==writes+1);
 a.bookCachePath.clear();a.didChangeStats=true;writes=Stats::writes;a.saveStats();assert(a.didChangeStats&&Stats::writes==writes);
 assert(messages>=6);
 std::cout<<"stats: actual save/date-exit/completion bodies, partial ACK failure+retry PASS\n";
}
