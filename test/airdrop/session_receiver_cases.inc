} // anonymous namespace
} // namespace airdrop
int main(int argc,char** argv){
  assert(argc==2);std::string mode=argv[1];
  using namespace airdrop;
  if(mode=="queue"){
    Offer offer;offer.kind=OfferKind::Url;OfferIdentity token{};token.generation=1;token.session=2;token.offer=3;token.connection=4;token.interfaceIndex=5;token.peer[15]=6;
    auto& receiver=Receiver::instance();pending(nullptr,offer,token,2000);
    for(unsigned field=0;field<7;++field){auto stale=token;switch(field){case 0:++stale.generation;break;case 1:++stale.session;break;case 2:++stale.offer;break;case 3:++stale.connection;break;case 4:++stale.interfaceIndex;break;case 5:++stale.peer[0];break;case 6:++stale.peer[15];break;}assert(!receiver.decide(stale,true));}
    assert(receiver.decide(token,true));assert(!receiver.decide(token,false));assert(decision(nullptr,token)==1);assert(!receiver.decide(token,true));assert(decision(nullptr,token)==0);
    ++token.offer;pending(nullptr,offer,token,2000);assert(receiver.decide(token,false));assert(decision(nullptr,token)==-1);assert(!receiver.decide(token,true));
    ++token.offer;pending(nullptr,offer,token,1000);assert(!receiver.decide(token,true));
    std::puts("PASS extracted real Receiver decision full tokens/stale/deadline/one-use after drain");return 0;
  }
  if(mode=="bind"){
    assert(sizeof(ifreq)==6);assert(bindInterface(7,"aw0"));assert(socketCalls==1);assert(!bindInterface(7,"abcdef"));assert(socketCalls==1);
    std::puts("PASS extracted real bindInterface pinned lwIP ifreq[6]/sizeof ABI");return 0;
  }
  if(mode.rfind("listen",0)==0){
    Context c;interfaceFailure=mode=="listen-interface-fail";socketBindFailure=mode=="listen-bind-fail";
    bool ok=listen(c);assert(ok==(!interfaceFailure&&!socketBindFailure));
    if(ok){assert(reused&&boundInterface);auto reuse=std::find(calls.begin(),calls.end(),"socket-reuse"),binding=std::find(calls.begin(),calls.end(),"socket-bind");assert(reuse<binding);}
    assert(c.listener==71);freeTls(c);assert(c.listener==-1&&closed==std::vector<int>({71}));
    std::puts("PASS extracted AWDL listener checked SO_REUSEADDR/interface binding/error cleanup ordering");return 0;
  }
  Context c;
  if(mode=="psa-fail")psaFailure=true;
  if(mode=="import-fail")importFailure=7;
  if(mode=="destroy-fail")destroyFailure=1;
  if(mode.rfind("tls:",0)==0)failName=mode.substr(4);
  bool ok=initTls(c);
  if(mode=="psa-fail"||mode=="import-fail"||mode=="destroy-fail"){
    assert(!ok&&budgetBinds==0&&!c.tlsInitialized);freeTls(c);assert(budgetEnds==0);
    if(mode=="destroy-fail"){
      assert(liveKeys.size()==1);destroyFailure=0;assert(preparePsa());assert(liveKeys.empty());
      std::puts("PASS failed owned PSA temporary ID retained and cleaned by later supported destroy");
    }else assert(liveKeys.empty());
    assert(chargedGlobals==0);return 0;
  }
  if(mode.rfind("tls:",0)==0)assert(!ok&&c.tlsInitialized);else assert(ok&&c.tlsInitialized);
  assert(chargedGlobals==0&&budgetBinds==1&&maxLive==16&&liveKeys.empty());
  auto firstBudget=std::find(calls.begin(),calls.end(),"budget-begin");auto firstPsa=std::find(calls.begin(),calls.end(),"psa-init");assert(firstPsa<firstBudget);
  c.client=21;c.listener=22;c.net.fd=21;freeTls(c);
  assert(closed==std::vector<int>({21,22})&&c.client==-1&&c.listener==-1&&c.net.fd==-1&&!c.tlsInitialized&&!c.budgetBound);
  assert(budgetEnds==1&&activeBudget==nullptr&&c.budget.used()==0);
  for(const char* name:{"mbedtls_ssl_free","mbedtls_ssl_config_free","mbedtls_pk_free","mbedtls_x509_crt_free","mbedtls_ctr_drbg_free","mbedtls_entropy_free"}){
    assert(std::count(calls.begin(),calls.end(),name)==1);assert(std::find(calls.begin(),calls.end(),name)<std::find(calls.begin(),calls.end(),"budget-end"));
  }
  freeTls(c);assert(closed.size()==2&&budgetEnds==1);
  if(mode=="cold"){
    unsigned before=imports;Context again;assert(initTls(again));assert(imports==before&&maxLive==16);freeTls(again);assert(chargedGlobals==0&&liveKeys.empty());
  }
  std::puts("PASS extracted real cold PSA-before-budget/TLS context cleanup ordering/idempotence (fake SDK boundaries)");
}
