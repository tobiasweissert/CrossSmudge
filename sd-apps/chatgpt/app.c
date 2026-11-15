#include "AppAbi.h"
#include "../common/Text.h"
#define CONTEXT 32768
#define DRAFT 8192
#define REPLY 16384
#define TURNS 64

typedef enum { IDLE, CHAT, VOICE } Job;
typedef struct {
  const CpHostV1* host;
  char draft[DRAFT+1];
  char history[CONTEXT+TURNS+1];
  uint32_t offsets[TURNS],lengths[TURNS];
  unsigned count,used,text_bytes;
  char request[CONTEXT+1];
  char conversation[CONTEXT+1024];
  char status[192];
  CpRow rows[9];
  CpMessage messages[CP_APP_MAX_MESSAGES];
  char title[64];
  int menu;
  int32_t sequence,active;
  Job job;
  int reading,confirm_clear,privacy;
} Chat;
typedef struct { char* out;size_t used,cap;int ok; } Writer;
static void append(Writer* w,const char* text) {
  size_t n=cp_length(text,w->cap);if(!w->ok || n>=w->cap || n>w->cap-w->used-1){w->ok=0;return;}
  memcpy(w->out+w->used,text,n+1);w->used+=n;
}
static void quote(Writer* w,const char* text) {
  static const char hex[]="0123456789abcdef";append(w,"\"");
  for(const unsigned char* p=(const unsigned char*)text;*p && w->ok;++p) {
    char one[7]={0};
    if(*p=='"' || *p=='\\'){one[0]='\\';one[1]=(char)*p;}
    else if(*p<32){one[0]='\\';one[1]='u';one[2]='0';one[3]='0';one[4]=hex[*p>>4];one[5]=hex[*p&15];}
    else one[0]=(char)*p;
    append(w,one);
  }
  append(w,"\"");
}
static void status(Chat* s,const char* text) {if(!cp_copy(s->status,sizeof(s->status),text))cp_copy(s->status,sizeof(s->status),"Operation failed");}
static const char* message(Chat* s,unsigned index){return index<s->count?s->history+s->offsets[index]:"";}
static void show(Chat* s) {
  if(s->host->version==CP_APP_ABI_V2 && !s->reading && !s->privacy && !s->confirm_clear && !s->menu) {
    unsigned n=s->count;
    for(unsigned i=0;i<n;++i)s->messages[i]=(CpMessage){i+1,(i&1)?CP_ASSISTANT:CP_USER,message(s,i),s->lengths[i]};
    if(s->job==CHAT)s->messages[n++]=(CpMessage){s->count+1,CP_USER,s->draft,(uint32_t)strlen(s->draft)};
    const char* title=s->count?message(s,0):"New conversation";
    size_t len=cp_length(title,sizeof(s->title)-1);
    while(len && ((unsigned char)title[len]&0xc0)==0x80)--len;
    memcpy(s->title,title,len);s->title[len]=0;
    for(size_t i=0;i<len;++i)if((unsigned char)s->title[i]<32)s->title[i]=' ';
    CpPresentation v={0};v.size=sizeof(v);v.kind=CP_CHAT_V2;v.identity=n;
    v.title=s->title;v.status=s->status;v.draft=s->draft;v.messages=s->messages;v.message_count=n;
    v.menu=12;v.new_chat=9;v.edit=1;v.send=2;v.hold=5;v.busy=s->active!=0;
    ((const CpHostV2*)s->host)->present_v2(s->host->context,&v);return;
  }
  s->rows[0]=(CpRow){1,"Edit message",0};s->rows[1]=(CpRow){2,"Send",0};
  s->rows[2]=(CpRow){5,s->job==VOICE?"Recording - release to review":"Hold to speak",1};
  s->rows[3]=(CpRow){3,"Read conversation",0};s->rows[4]=(CpRow){9,"New chat",0};
  s->rows[5]=(CpRow){4,"Clear",0};s->rows[6]=(CpRow){6,"Wi-Fi",0};
  s->rows[7]=(CpRow){7,"API key settings",0};s->rows[8]=(CpRow){8,"Privacy",0};
  const char* text=s->count>=2?message(s,s->count-2):"You: type a message or hold to speak";
  const char* assistant=s->count?message(s,s->count-1):"ChatGPT replies appear here. Tap a bubble for the full conversation.";
  CpView view={sizeof(view),CP_CHAT,"ChatGPT",s->status,text,assistant,s->draft,s->rows,9};
  if(s->menu)view.kind=CP_LIST;
  if(s->confirm_clear){s->rows[0]=(CpRow){10,"Delete local conversation and draft",0};s->rows[1]=(CpRow){11,"Keep conversation",0};view.kind=CP_LIST;view.row_count=2;}
  if(s->reading || s->privacy){view.kind=CP_READING;view.text=s->privacy?
    "Messages and audio are sent only to api.openai.com. Transcripts are reviewed before a separate Send. The API key is stored in device NVS, which is not encrypted. Removal is logical deletion, not secure erasure. Clearing this conversation does not delete provider records. Requests may incur charges.":s->conversation;}
  s->host->present(s->host->context,&view);
}
static int build_request(Chat* s) {
  Writer w={s->request,0,sizeof(s->request),1};s->request[0]=0;
  append(&w,"{\"model\":\"gpt-4.1-mini\",\"store\":false,\"messages\":[");
  for(unsigned i=0;i<=s->count;++i) {
    if(i)append(&w,",");
    append(&w,(i&1)?"{\"role\":\"assistant\",\"content\":" : "{\"role\":\"user\",\"content\":");
    quote(&w,i==s->count?s->draft:message(s,i));append(&w,"}");
  }
  append(&w,"]}");return w.ok;
}
static void read_conversation(Chat* s) {
  Writer w={s->conversation,0,sizeof(s->conversation),1};s->conversation[0]=0;
  for(unsigned i=0;i<s->count;++i){append(&w,(i&1)?"Assistant:\n":"You:\n");append(&w,message(s,i));append(&w,"\n\n");}
  if(!s->count)append(&w,"No messages yet. Type or record, review the draft, then press Send.");
  s->reading=1;
}
static int valid_text(const char* text,size_t length,size_t cap) {
  if(!text || length>cap)return 0;
  for(size_t i=0;i<length;++i)if(!text[i])return 0;
  return 1;
}
static int start(const CpHostV1* h,void** out) {
  if(!h || !((h->size==sizeof(*h) && h->version==CP_APP_ABI_V1) || (h->size==sizeof(CpHostV2) && h->version==CP_APP_ABI_V2)) || !out)return 0;
  Chat* s=h->allocate(h->context,sizeof(*s));if(!s)return 0;memset(s,0,sizeof(*s));s->host=h;*out=s;
  status(s,"Configure the OpenAI API key in Settings before sending");show(s);return 1;
}
static int32_t ticket(Chat* s){if(s->sequence==INT32_MAX)s->sequence=0;return ++s->sequence;}
static void event(void* state,const CpEvent* e) {
  Chat* s=state;const CpHostV1* h=s->host;
  if(!e || e->size!=sizeof(*e))return;
  if(e->kind==CP_CANCELLED || e->kind==CP_BACK) {
    if(s->active){h->cancel(h->context);s->active=0;s->job=IDLE;status(s,"Cancelled; unsent draft preserved");}
    else if(s->reading || s->privacy || s->confirm_clear || s->menu)s->reading=s->privacy=s->confirm_clear=s->menu=0;
    else {h->leave(h->context);return;}
    show(s);return;
  }
  if(e->kind==CP_HOLD_END){if(s->active && s->job==VOICE)h->record(h->context,s->active,0);return;}
  if(e->kind==CP_HOLD_START && !s->active) {
    if(s->draft[0]){status(s,"Existing draft preserved; send or clear it before recording");show(s);return;}
    s->active=ticket(s);s->job=VOICE;
    if(!h->record(h->context,s->active,1)){s->active=0;s->job=IDLE;status(s,"Recording unavailable; check Wi-Fi and API key settings");}
    else status(s,"Recording; release to transcribe and review");
    show(s);return;
  }
  if(e->kind==CP_HTTP_DONE) {
    if(!s->active || s->active!=e->id)return;
    Job job=s->job;s->active=0;s->job=IDLE;
    if(e->status!=200 || !e->length || !valid_text(e->text,e->length,job==VOICE?DRAFT:REPLY)) {
      status(s,"Request failed; draft and conversation preserved");show(s);return;
    }
    if(job==VOICE) {
      memcpy(s->draft,e->text,e->length);s->draft[e->length]=0;
      status(s,"Review transcription, then press Send explicitly");show(s);
      h->edit(h->context,1,"Review transcript",s->draft,DRAFT);return;
    }
    size_t draft_length=strlen(s->draft);
    if(s->count+2>TURNS || draft_length+e->length>CONTEXT-s->text_bytes) {
      status(s,"Conversation full; draft preserved. Start a new chat.");show(s);return;
    }
    s->offsets[s->count]=s->used;s->lengths[s->count++]=draft_length;
    memcpy(s->history+s->used,s->draft,draft_length+1);s->used+=draft_length+1;
    s->offsets[s->count]=s->used;s->lengths[s->count++]=e->length;
    memcpy(s->history+s->used,e->text,e->length);s->used+=e->length;s->history[s->used++]=0;
    s->text_bytes+=draft_length+e->length;s->draft[0]=0;
    status(s,"Reply received; conversation stays in RAM");show(s);return;
  }
  if(e->kind==CP_EDITED && !s->active && e->id==1) {
    if(e->status==0 && valid_text(e->text,e->length,DRAFT)){memcpy(s->draft,e->text,e->length);s->draft[e->length]=0;}
    show(s);return;
  }
  if(e->kind==CP_WIFI_DONE){status(s,e->status?"Wi-Fi connected":"Wi-Fi not connected");show(s);return;}
  if(e->kind!=CP_SELECT || s->active)return;
  switch(e->id) {
    case 12:s->menu=1;break;
    case 1:s->menu=0;h->edit(h->context,1,"Message",s->draft,DRAFT);break;
    case 2:
      if(!s->draft[0])status(s,"Nothing to send; type or record a draft first");
      else if(s->count>=TURNS || !build_request(s))status(s,"Context limit reached; start a new chat");
      else {
        s->active=ticket(s);s->job=CHAT;
        if(!h->request(h->context,s->active,CP_PROVIDER_CHAT,0,(const uint8_t*)s->request,strlen(s->request))) {
          s->active=0;s->job=IDLE;status(s,"Could not send; check Wi-Fi and API key settings");
        } else status(s,"Waiting for ChatGPT - Back cancels");
      }
      break;
    case 3:read_conversation(s);break;
    case 4:case 9:s->confirm_clear=1;status(s,"Clear local RAM conversation and draft? Provider records are unaffected.");break;
    case 5:status(s,"Hold the voice control while speaking; release to review");break;
    case 6:h->wifi(h->context);break;
    case 7:h->key_settings(h->context);break;
    case 8:s->privacy=1;break;
    case 10:
      if(s->confirm_clear){memset(s->history,0,sizeof(s->history));memset(s->draft,0,sizeof(s->draft));
        s->count=s->used=s->text_bytes=0;s->confirm_clear=s->menu=0;status(s,"Local conversation cleared; provider retention is separate");}
      break;
    case 11:s->confirm_clear=0;break;
    default:break;
  }
  show(s);
}
static void stop(void* state){Chat* s=state;const CpHostV1* h=s->host;memset(s,0,sizeof(*s));h->release(h->context,s);}
int app_main(int argc,char** argv) {
  if(argc!=1 || !argv || !argv[0])return -1;
  CpBootstrap* b=(CpBootstrap*)argv[0];if(b->size!=sizeof(*b) || (b->version!=CP_APP_ABI_V1 && b->version!=CP_APP_ABI_V2))return -1;
  b->module=(CpModuleV1){sizeof(CpModuleV1),b->version,start,event,stop};return 0;
}
