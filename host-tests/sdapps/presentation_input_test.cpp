#define main raster_fixture_main
#include "presentation_raster.cpp"
#undef main
int main(){
 auto model=std::make_unique<Model>();auto& m=*model;strcpy(m.text,"one link then another link");m.length=strlen(m.text);m.spans[0]={4,8,1000};m.spans[1]={22,26,1001};m.spanCount=2;m.binding.kind=CP_BROWSER_V2;
 Raster target(480,800);ui::ThemeTokens theme;ui::InteractionBuffer<24> controls;
 controls.beginPublishCycle();drawScreen(m,target,theme,rect(0,0,480,800),"Ask anything","Send",[&](ui::Rect r,Control value){ui::Interaction i{};i.rect=r;i.action=2;i.value=static_cast<int16_t>(value);i.inputMask=ui::InputTouch;assert(controls.addInteraction(i));});controls.publish();
 assert(!controls.publishedOverflowed()&&m.hitCount==2);
 unsigned fragments=0;for(const auto& text:target.drawn)if(text.first=="link"){
   assert(fragments<m.hitCount);assert(text.second.x==m.hits[fragments].bounds.x && text.second.width==m.hits[fragments].bounds.width);++fragments;
 }assert(fragments==2);
 for(uint32_t i=0;i<m.hitCount;++i){auto r=m.hits[i].bounds;ui::InputSnapshot input{};input.touchReleased=true;input.touchX=r.x+r.width/2;input.touchY=r.y+r.height/2;auto event=controls.routePublished(input);assert(event.action==2&&event.value==6);assert(m.hits[i].action==1000+int(i));}
 for(size_t i=0;i<controls.publishedCount();++i){auto control=controls.publishedData()[i];ui::InputSnapshot input{};input.touchReleased=true;input.touchX=control.rect.x;input.touchY=control.rect.y;auto event=controls.routePublished(input);assert(event.action==2&&event.value==control.value);}
}
