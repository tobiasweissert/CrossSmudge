// Production Session + HTTP + Stream regressions for MAIN-PROTOCOL-001.
// Scripted transport/consent/storage: no physical TLS/SD qualification.
void framingCases(const std::string& root,const std::vector<uint8_t>& file){
  unsigned cases=0;
  for(const char* fixture:{"stored.dvzip","zlib.dvzip","stored-1.dvzip","zlib-1.dvzip","stored-89.dvzip","zlib-89.dvzip","stored-176.dvzip","zlib-176.dvzip"}){
    auto full=load(root,fixture);
    for(bool endRecord:{false,true}){
      auto archive=full;if(!endRecord)archive.resize(archive.size()-4);
      for(unsigned mode=0;mode<14;++mode){
        auto data=archive;
        // Invalid *inside* the declared body/chunk, including partial next record.
        if(mode==2||mode==3)data.push_back(0xff);
        if(mode==4)data.insert(data.end(),{0,0,0});
        std::vector<uint8_t> upload;size_t cutoff=0;
        if(mode==0||mode==2||mode==4)upload=request("/Upload",data,uploadHeaders);
        else if(mode==1){upload=request("/Upload",data,uploadHeaders);std::string text(upload.begin(),upload.end());auto at=text.find("Content-Length: ")+16;auto end=text.find("\r\n",at);text.replace(at,end-at,std::to_string(data.size()+100));upload.assign(text.begin(),text.end());}
        else {
          upload=request("/Upload",data,uploadHeaders,true);upload.resize(upload.size()-5); // complete nonzero chunk+CRLF
          if(mode==5)upload.resize(upload.size()-2); // missing CRLF
          if(mode==6)upload.resize(upload.size()-1); // partial CRLF
          if(mode==7){ // declared chunk longer than supplied complete archive
            std::string text(upload.begin(),upload.end());auto at=text.find("\r\n\r\n")+4;auto end=text.find("\r\n",at);char n[32];snprintf(n,sizeof n,"%zx",data.size()+100);text.replace(at,end-at,n);upload.assign(text.begin(),text.end());upload.resize(upload.size()-2);
          }
          if(mode==8||mode==9||mode==10){cutoff=upload.size();std::string tail=mode==8?"0\r\n\r\n":mode==9?"1\r\nX\r\n0\r\n\r\n":"garbage\r\nPOST /Ask HTTP/1.1\r\nContent-Length: 0\r\n\r\n";upload.insert(upload.end(),tail.begin(),tail.end());}
          if(mode==11||mode==12){ // two chunks: first ends before the archive/record completes
            size_t half=data.size()/2;auto first=std::vector<uint8_t>(data.begin(),data.begin()+half);upload=request("/Upload",first,uploadHeaders,true);upload.resize(upload.size()-5);char n[32];snprintf(n,sizeof n,"%zx\r\n",data.size()-half);upload.insert(upload.end(),n,n+strlen(n));upload.insert(upload.end(),data.begin()+half,data.end());upload.insert(upload.end(),{'\r','\n'});
            if(mode==12)upload.pop_back();
          }
          if(mode==13){ // checksum/data corruption remains fatal
            auto at=std::search(upload.begin(),upload.end(),data.begin(),data.end());assert(at!=upload.end());*(at+data.size()-(endRecord?5:1))^=1;
          }
        }
        bool success=mode==0||mode==8||mode==9||mode==10||mode==11;
        // Exhaust every two-read split, plus one-byte TLS reads and coalesced 2048.
        for(size_t split=0;split<=upload.size()+1;++split){
          Harness h;h.input=request("/Ask",file);size_t start=h.input.size();add(h.input,upload);h.waitMs=1000;h.eofDisconnect=true;
          if(split==upload.size()+1)h.split=1;else if(split)h.readBoundary=start+split;
          h.serve();++cases;
          bool ok=success?(h.finishes==1&&h.count(200)==2&&h.cancels==0&&h.responseCloses&&h.pendingCount==1&&std::string(h.output.begin(),h.output.end())=="abc"):(h.finishes==0&&h.count(200)==1&&h.cancels==1);
          if(!ok&&failures<20)fprintf(stderr,"framing fixture=%s end=%d mode=%u split=%zu finish=%u ack=%u cancel=%u\n",fixture,endRecord,mode,split,h.finishes,h.count(200),h.cancels);
          if(!ok)++failures;
          // Coalesced and later bytes after the cutoff are abandoned, not validated.
          if(success&&cutoff&&h.finishes){CHECK("exact cutoff regardless read segmentation",h.position-(h.workspace->end-h.workspace->begin)==start+cutoff);}
        }
      }
    }
  }
  auto data=load(root,"stored.dvzip");data.resize(data.size()-4);
  for(unsigned fault=0;fault<5;++fault){
    Harness h;h.answerAfter=1;h.input=request("/Ask",file);auto upload=request("/Upload",data,uploadHeaders,true);upload.resize(upload.size()-6);add(h.input,upload);h.waitMs=1000;
    if(fault==1)h.eofDisconnect=true;if(fault==2)h.cancelOnWait=4;if(fault==3)h.disconnectOnWait=4;if(fault==4)h.waitMs=1800000;
    h.serve();CHECK("partial CRLF stall EOF cancel permission/channel loss deadline cannot publish",h.finishes==0&&h.cancels==1&&h.count(200)==1);
  }
  for(unsigned fault=0;fault<3;++fault){
    Harness h;h.answerAfter=1;h.input=request("/Ask",file);add(h.input,request("/Upload",data,uploadHeaders,true));h.blockCount=2000000;h.waitMs=1000;
    if(fault==0)h.cancelOnWait=4;if(fault==1)h.disconnectOnWait=4;
    h.serve();CHECK("sustained sink WouldBlock cancellation channel loss deadline",h.finishes==0&&h.cancels==1&&h.count(200)==1);
  }
  printf("MAIN-PROTOCOL-001 framing cases=%u (stored/zlib; every two-read split, bytewise, coalesced)\n",cases);
}
