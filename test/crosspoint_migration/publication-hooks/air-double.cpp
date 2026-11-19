enum class State{Publishing,Success,Error};namespace storage{enum class State{Uncertain,Other};}
int lockDepth=0,statusMux=0;bool finishOK=true,uncertain=false;
#define portENTER_CRITICAL(x) (++lockDepth)
#define portEXIT_CRITICAL(x) (--lockDepth)
struct Sink{std::string path="/received.epub";bool finish(){assert(lockDepth==0);return finishOK;}const char* savedPath(){return path.c_str();}unsigned committedBytes(){return 12;}storage::State state(){return uncertain?storage::State::Uncertain:storage::State::Other;}const char*error(){return "synthetic failure";}};
struct Context{Sink storage;bool fileCommitted=false;};struct {unsigned committedBytes=0;char savedPath[256]{};State state=State::Error;bool publicationUncertain=false;int revision=0;}status;
void state(State s){status.state=s;}void failure(Context&,const char*){status.state=State::Error;}
size_t strlcpy(char*to,const char*from,size_t n){auto len=strlen(from);if(n){auto take=std::min(n-1,len);memcpy(to,from,take);to[take]=0;}return len;}
