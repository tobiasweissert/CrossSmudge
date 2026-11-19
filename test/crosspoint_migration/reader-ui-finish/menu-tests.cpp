int main(){using A=XtcReaderMenuActivity::MenuAction;
 for(bool chapter:{false,true})for(bool touch:{false,true})for(bool track:{false,true}){
 SETTINGS.track=track;auto rows=XtcReaderMenuActivity::buildMenuItems(chapter,false,touch);assert(rows.size()<=8);
 XtcReaderMenuActivity m;m.items=rows;m.statsEnabled=false;m.refreshListItems();bool found=false;
 for(size_t i=0;i<rows.size();++i)if(rows[i].action==A::TOGGLE_STATS){found=true;assert(std::string(m.listItems[i].value)=="Off");m.selectedIndex=i;m.selectCurrent();assert(m.finished&&m.result==static_cast<int>(A::TOGGLE_STATS));}
 assert(found==track);
 }
 ReaderDispatch r;r.onReaderMenuConfirm(static_cast<int>(A::TOGGLE_STATS));assert(!r.enabled&&r.resumes==1&&r.updates==1);
 r.ack=false;r.onReaderMenuConfirm(static_cast<int>(A::TOGGLE_STATS));assert(!r.enabled&&toasts==1&&r.resumes==2&&r.updates==2);
 std::cout<<"menu: actual row build/values/select/reader dispatch, availability and failed-toggle PASS\n";
}
