// No actual fonts/PNG codec/panel pixels are simulated or qualified here.
int main(){
 int cases=0;
 auto reset=[](){files.clear();decodeCalls=failDecode=0;dimsOk=true;oom=false;SleepCoverAssets::cached=false;SleepCoverAssets::prepareOk=true;APP_STATE.openEpubPath.clear();SETTINGS.sleepScreen=12;SETTINGS.sleepScreenCoverMode=0;SETTINGS.sleepScreenCoverFilter=0;};
 auto bmp=[](const char*path){auto&v=files[path].data;v.assign(70,0);auto le=[&](int o,uint32_t n,int bytes=4){for(int i=0;i<bytes;++i)v[o+i]=n>>(8*i);};le(0,0x4d42,2);le(10,54);le(14,40);le(18,4);le(22,1);le(26,1,2);le(28,32,2);for(int i=0;i<4;++i){v[54+4*i]=v[55+4*i]=v[56+4*i]=i*85;v[57+4*i]=i?255:0;}};
 for(int hardware=0;hardware<3;++hardware)for(int filter=0;filter<3;++filter)for(int fit=0;fit<2;++fit)for(int cover=0;cover<2;++cover)for(int png=0;png<2;++png){
  reset();GfxRenderer r;r.absolute=hardware!=0;r.direct=hardware==2;SleepActivity a(r);SETTINGS.sleepScreen=cover?13:12;SETTINGS.sleepScreenCoverMode=fit;SETTINGS.sleepScreenCoverFilter=filter;
  if(png)files["/sleep-overlay.png"]={};else bmp("/sleep-overlay.bmp");
  if(cover){APP_STATE.openEpubPath="/book.epub";bmp("/cover.bmp");a.renderCoverSleepScreen();assert(r.stores>=1&&r.restores>=1);assert(a.overlayCover==nullptr&&a.overlayGrayCover==nullptr);}else a.renderTransparentCustomSleepScreen();
  assert(a.defaults==0);assert(r.gray==1&&r.lsb==1&&r.msb==1);assert(r.mode==GfxRenderer::BW&&r.poweredOff);if(png)assert(decodeCalls==3);else assert(r.pixels>0);++cases;
 }
 // PNG failure on either grayscale pass must stop, not decode another candidate
 // with a half-built mask. All paths must return the renderer to BW.
 for(int hardware=0;hardware<3;++hardware)for(int failure=1;failure<=3;++failure){reset();GfxRenderer r;r.absolute=hardware!=0;r.direct=hardware==2;SleepActivity a(r);files["/sleep-overlay.png"]={};failDecode=failure;a.renderTransparentCustomSleepScreen();assert(r.mode==GfxRenderer::BW);if(failure==1)assert(a.defaults==1);else assert(r.gray==0&&a.defaults==0);++cases;}
 // Snapshot denial refuses decode rather than risking an unrecoverable underlay.
 reset();{GfxRenderer r;r.absolute=r.direct=false;r.storeOk=false;SleepActivity a(r);files["/sleep-overlay.png"]={};a.renderTransparentCustomSleepScreen();assert(a.defaults==1&&r.bw==1&&r.gray==0&&decodeCalls==0&&r.mode==GfxRenderer::BW&&r.poweredOff);++cases;}
 reset();{GfxRenderer r;SleepActivity a(r);bmp("/sleep-overlay.bmp");oom=true;a.renderTransparentCustomSleepScreen();assert(a.defaults==1&&r.mode==GfxRenderer::BW);++cases;}
 // Missing overlay: bear fallback, or plain cover, never a dangling borrow.
 for(bool cover:{false,true}){reset();SETTINGS.sleepScreen=cover?13:12;GfxRenderer r;SleepActivity a(r);if(cover){APP_STATE.openEpubPath="/book.epub";bmp("/cover.bmp");a.renderCoverSleepScreen();assert(a.defaults==0&&r.gray==1);}else{a.renderTransparentCustomSleepScreen();assert(a.defaults==1);}assert(!a.overlayCover&&!a.overlayGrayCover);++cases;}
 // A failed loaning cover build restores the original underlay before overlay.
 reset();{GfxRenderer r;SleepActivity a(r);APP_STATE.openEpubPath="/book.epub";SleepCoverAssets::prepareOk=false;files["/sleep-overlay.png"]={};SETTINGS.sleepScreen=13;a.renderCoverSleepScreen();assert(r.restores==1&&r.frame==7&&r.gray==1);assert(!a.overlayCover&&!a.overlayGrayCover);++cases;}
 // No snapshot memory: do not call a builder that may loan the retained frame.
 reset();{GfxRenderer r;r.storeOk=false;SleepActivity a(r);APP_STATE.openEpubPath="/book.epub";files["/sleep-overlay.png"]={};SETTINGS.sleepScreen=13;a.renderCoverSleepScreen();assert(!SleepCoverAssets::cached&&a.defaults==1&&r.gray==0&&decodeCalls==0);++cases;}
 // Directory fallback exercises actual native selector and separate overlay state.
 reset();{GfxRenderer r;SleepActivity a(r);bmp("/.sleep-overlay/art.bmp");files["/.sleep-overlay"].dir=true;files["/.sleep-overlay"].children={"/.sleep-overlay/art.bmp"};int before=APP_STATE.saves;a.renderTransparentCustomSleepScreen();assert(a.defaults==0&&APP_STATE.saves==before+1);++cases;}
 // Parser bounds and short rows execute production source, not decoder doubles.
 reset();{GfxRenderer r;bmp("/bad.bmp");HalFile f("/bad.bmp");OverlayBmpInfo info;assert(parseOverlayBmpHeader(f,info,false));files[f.path].data.resize(58);uint8_t row[16];assert(!renderTransparentOverlayPass(f,info,{},r,row,TransparentOverlayPass::BW));++cases;}
 // Actual per-row alpha/luminance/plane writes, logged as synthetic operations, not panel pixels.
 reset();{GfxRenderer r;bmp("/alpha.bmp");HalFile f("/alpha.bmp");OverlayBmpInfo info;assert(parseOverlayBmpHeader(f,info,false));uint8_t row[16];assert(renderTransparentOverlayPass(f,info,{},r,row,TransparentOverlayPass::BW));assert((r.writes==std::vector<std::pair<int,bool>>{{1,true},{2,true},{3,false}}));r.writes.clear();r.bases=1;assert(renderTransparentOverlayPass(f,info,{},r,row,TransparentOverlayPass::GrayscaleLsb));assert((r.writes==std::vector<std::pair<int,bool>>{{1,false},{2,true},{3,false}}));r.writes.clear();assert(renderTransparentOverlayPass(f,info,{},r,row,TransparentOverlayPass::GrayscaleMsb));assert((r.writes==std::vector<std::pair<int,bool>>{{1,true},{2,false},{3,false}}));++cases;}
 // Fit/crop are exercised with genuinely oversize dimensions, not just setting tokens.
 reset();{GfxRenderer r;auto p=calculateBitmapPlacement(8,4,r);assert(p.x==0&&p.y==1&&p.cropX==0);SETTINGS.sleepScreenCoverMode=1;p=calculateBitmapPlacement(8,4,r);assert(p.x==0&&p.y==0&&p.cropX==0.5f);p=calculateBitmapPlacement(4,8,r);assert(p.x==0&&p.y==0&&p.cropY==0.5f);++cases;}
 // Regular BMP white-key composition follows the real orchestration branch.
 reset();{GfxRenderer r;SleepActivity a(r);bmp("/sleep-overlay.bmp");files["/sleep-overlay.bmp"].data[28]=24;a.renderTransparentCustomSleepScreen();assert(r.draws==3&&r.pixels==0&&r.gray==1&&a.defaults==0);++cases;}
 // The modern pipeline never reports a completed grayscale frame after base rejection.
 reset();{GfxRenderer r;r.baseOk=false;SleepActivity a(r);files["/sleep-overlay.png"]={};a.renderTransparentCustomSleepScreen();assert(r.gray==0&&r.mode==GfxRenderer::BW&&a.defaults==1);++cases;}
 // Initial decode failure must not poison the retained underlay used by another candidate.
 reset();{GfxRenderer r;SleepActivity a(r);failDecode=1;assert(!a.renderTransparentOverlayPng("/bad.png"));assert(r.frame==7&&r.mode==GfxRenderer::BW);++cases;}
 printf("composition: %d synthetic real-body cases PASS (not physical/pixel evidence)\n",cases);
}
