template<class Reader>void exercise(){
 for(bool choose:{false,true}){
  auto owned=std::make_unique<Reader>();auto* reader=owned.get();activityManager.currentActivity=std::move(owned);
  nowMs=1000;activityManager.setReaderSessionActive(*reader,true);nowMs=11000;reader->readerSession.noteTurn(true,true);reader->readerSession.onRenderComplete(nowMs,trustedtime::trustedNow(),100);
  if constexpr(std::is_same_v<Reader,EpubReaderActivity>)reader->openQuickActionsPopup();else reader->handleShortcutAction(CrossPointSettings::SHORT_PWRBTN::QUICK_ACTIONS);
  assert(reader->quickActionsPopup.isActive()&&reader->mappedInput.overrideTouch&&reader->readerSession.durationSeconds()==0);
  nowMs=3011000;reader->quickActionsPopup.close(choose);assert(!reader->mappedInput.overrideTouch);
  nowMs+=10000;reader->readerSession.noteTurn(true,true);reader->readerSession.onRenderComplete(nowMs,trustedtime::trustedNow(),200);assert(reader->readerSession.durationSeconds()==10);
 }
}
int main(){exercise<EpubReaderActivity>();exercise<XtcReaderActivity>();exercise<TxtReaderActivity>();std::cout<<"popup: all three actual production open/cancel/select callbacks with real session clock PASS\n";}
