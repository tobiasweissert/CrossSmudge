#pragma once
#include "AppAbi.h"
#include <cstring>
namespace sdapps::presentation {
inline bool utf8(const char* text,size_t n) {
  if(!text)return n==0;
  for(size_t i=0;i<n;){
    const uint8_t c=text[i++];if(c<128){if(!c)return false;continue;}
    unsigned count=c>=0xf0&&c<=0xf4?3:c>=0xe0&&c<=0xef?2:c>=0xc2&&c<=0xdf?1:0;
    if(!count || count>n-i)return false;
    const uint8_t first=text[i];
    if((c==0xe0&&first<0xa0)||(c==0xed&&first>=0xa0)||(c==0xf0&&first<0x90)||(c==0xf4&&first>=0x90))return false;
    while(count--)if((uint8_t(text[i++])&0xc0)!=0x80)return false;
  }return true;
}
inline bool validPresentation(const CpPresentation* v) {
  if(!v || v->size!=sizeof(*v) || (v->kind!=CP_BROWSER_V2 && v->kind!=CP_CHAT_V2) ||
     v->message_count>CP_APP_MAX_MESSAGES || (v->message_count && !v->messages) || v->busy>1)return false;
  const int32_t actions[]={v->back,v->address_action,v->menu,v->new_chat,v->edit,v->send,v->hold,v->link_base};
  for(auto action:actions)if(action<0 || action>INT32_MAX-128)return false;
  const char* strings[]={v->title,v->address,v->status,v->draft};const size_t caps[]={64,2049,192,8193};
  for(unsigned i=0;i<4;++i)if(strings[i]){size_t n=strnlen(strings[i],caps[i]);if(n==caps[i] || !utf8(strings[i],n))return false;}
  uint32_t total=0;
  for(uint32_t i=0;i<v->message_count;++i){const auto& m=v->messages[i];
    if((m.role!=CP_USER && m.role!=CP_ASSISTANT) || !m.text || m.length>32768 || m.length>40960-total ||
       strnlen(m.text,m.length+1)!=m.length || !utf8(m.text,m.length))return false;
    for(uint32_t j=0;j<i;++j)if(m.id==v->messages[j].id)return false;
    total+=m.length;
  }
  return true;
}
}
