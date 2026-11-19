void turn(ReaderActivity& r,uint32_t when,int progress){nowMs=when;r.progress=progress;r.readerSession.noteTurn(true,true);r.readerSession.onRenderComplete(millis(),trustedtime::trustedNow(),progress);}
int main(){
 ActivityManager manager;auto& r=*manager.currentActivity;
 manager.stackActivities.push_back(std::make_unique<ReaderActivity>());
 manager.notifyInputLockChanged(true);manager.notifyInputLockChanged(false);
 assert(r.pauses==1&&r.resumes==1&&manager.stackActivities[0]->pauses==1&&manager.stackActivities[0]->resumes==0);
 nowMs=1000;manager.setReaderSessionActive(r,true);
 turn(r,11000,100);assert(r.readerSession.durationSeconds()==10);
 manager.inputLocked=true;manager.setReaderSessionActive(r,false);
 assert(pluginevents::durations==std::vector<uint32_t>{10});
 nowMs=3011000;manager.inputLocked=false;
 // Unlock restores only the saved badge: no repaint. This must reanchor now.
 manager.setReaderSessionActive(r,true);turn(r,3021000,200);
 assert(r.readerSession.durationSeconds()==10);
 manager.setReaderSessionActive(r,false);assert(pluginevents::durations==std::vector<uint32_t>({10,10}));
 manager.setReaderSessionActive(r,false);assert(pluginevents::durations.size()==2);
 // Same-task owner lock: the helper must not recursively acquire RenderLock.
 holder=1;int oldLocks=locks;manager.setReaderSessionActive(r,true);assert(locks==oldLocks&&holder==1);holder=0;
 nowMs+=600000;r.overlay=true;manager.setReaderSessionActive(r,false);
 r.overlay=false;manager.setReaderSessionActive(r,true);turn(r,nowMs+10000,300);assert(r.readerSession.durationSeconds()==10);
 ReaderActivity stacked;nowMs+=500000;manager.setReaderSessionActive(stacked,true);turn(stacked,nowMs+10000,10);assert(stacked.readerSession.durationSeconds()==0);
 r.painted=false;manager.setReaderSessionActive(r,true);turn(r,nowMs+10000,400);assert(r.readerSession.durationSeconds()==0);
 pluginevents::subscriber=false;manager.setReaderSessionActive(r,false);assert(r.readerSession.durationSeconds()==0);
 std::cout<<"session: actual manager+flush and real ReaderSession, no-paint unlock / overlay / held-lock PASS\n";
}
