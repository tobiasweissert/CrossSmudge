#ifndef CROSSPLAY_APP_ABI_H
#define CROSSPLAY_APP_ABI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CP_APP_ABI_V1 1u
#define CP_APP_MAX_ROWS 448u
#define CP_APP_MAX_TEXT 196608u
#define CP_APP_MAX_DRAFT 8192u

typedef enum { CP_SELECT, CP_EDITED, CP_HTTP_DONE, CP_CANCELLED, CP_HOLD_START, CP_HOLD_END, CP_WIFI_DONE, CP_BACK } CpEventKind;
typedef enum { CP_LIST, CP_READING, CP_CHAT } CpViewKind;
typedef enum { CP_GET, CP_PROVIDER_CHAT, CP_PROVIDER_TRANSCRIBE } CpRequestKind;
// flags bit 0: press/hold/release events instead of ordinary activation.
typedef struct { int32_t id; const char* label; uint32_t flags; } CpRow;
typedef struct {
  uint32_t size;
  // CP_CHAT rows: edit, send, hold, read conversation, new, clear, Wi-Fi,
  // key settings, privacy. Text/assistant_text form the two preview bubbles.
  CpViewKind kind;
  const char* title;
  const char* status;
  const char* text;
  const char* assistant_text;
  const char* draft;
  const CpRow* rows;
  uint32_t row_count;
} CpView;
typedef struct {
  uint32_t size;
  CpEventKind kind;
  int32_t id;
  int32_t status;
  const char* text;
  size_t length;
  const char* location;
} CpEvent;
typedef struct CpHostV1 {
  uint32_t size;
  uint32_t version;
  void* context;
  void* (*allocate)(void*, size_t);
  void (*release)(void*, void*);
  int (*present)(void*, const CpView*);
  int (*edit)(void*, int32_t, const char*, const char*, size_t);
  // Nonzero means accepted, not completed. Only one job/editor at a time.
  // GET: URL <=2048; provider chat: URL ignored, body <=32768. The host
  // alone supplies provider keys and origin. Transcription uses record().
  int (*request)(void*, int32_t, CpRequestKind, const char*, const uint8_t*, size_t);
  void (*cancel)(void*);
  int (*record)(void*, int32_t, int);
  int (*wifi)(void*);
  int (*key_settings)(void*);
  int (*read_file)(void*, const char*, void*, size_t, size_t*);
  int (*write_file)(void*, const char*, const void*, size_t);
  uint32_t (*milliseconds)(void*);
  void (*leave)(void*);
  uint32_t (*parse_html)(void*, const char*, const char*, size_t);
  // field: 0 title, 1 body, 2 link label, 3 link URL. Borrowed until release.
  const char* (*document_field)(void*, uint32_t, uint32_t, uint32_t);
  uint32_t (*document_links)(void*, uint32_t);
  void (*release_document)(void*, uint32_t);
  int (*resolve_url)(void*, const char*, const char*, char*, size_t);
  int (*cache_list)(void*, char*, size_t);
  int (*cache_read)(void*, const char*, char*, size_t, size_t*);
  int (*cache_write)(void*, const char*, const char*, size_t, const char*);
  int (*cache_clear)(void*);
} CpHostV1;
// V2 embeds the unchanged V1 service prefix. V1 clients receive size/version 1;
// V2 clients receive sizeof(CpHostV2)/2 and may call present_v2.
#define CP_APP_ABI_V2 2u
#define CP_APP_MAX_MESSAGES 65u
#define CP_APP_MAX_SPANS 2048u
typedef enum { CP_BROWSER_V2, CP_CHAT_V2 } CpPresentationKind;
typedef enum { CP_USER, CP_ASSISTANT } CpMessageRole;
typedef struct { uint32_t id; uint32_t role; const char* text; uint32_t length; } CpMessage;
typedef struct {
  uint32_t size;
  uint32_t kind;
  // Stable content revision: document identity for Browser, turn revision for
  // Chat. Status/draft-only updates retain it; new chat content follows its tail.
  uint32_t identity;
  uint32_t document;
  const char* title;
  const char* address;
  const char* status;
  const char* draft;
  const CpMessage* messages;
  uint32_t message_count;
  // Positive controller-owned action IDs. Zero disables an action.
  int32_t back, address_action, menu, new_chat, edit, send, hold, link_base;
  uint32_t busy;
} CpPresentation;
typedef struct { CpHostV1 v1; int (*present_v2)(void*, const CpPresentation*); } CpHostV2;
typedef struct {
  uint32_t size;
  uint32_t version;
  // On failure, leave state null or safe for stop(); do not retain workers.
  int (*start)(const CpHostV1*, void**);
  void (*event)(void*, const CpEvent*);
  void (*stop)(void*);
} CpModuleV1;
typedef struct {
  uint32_t size;
  uint32_t version;
  CpModuleV1 module;
} CpBootstrap;
// app_main receives argc=1 and argv[0] pointing to a CpBootstrap. It only
// fills the descriptor. All subsequent callbacks run on the host UI task.
// Borrowed strings are valid for a call only; no STL objects cross this ABI.
// Host workers never execute module callbacks. cancel/drain precedes stop,
// and stop precedes freeing executable memory. Native code remains trusted.
#ifdef __cplusplus
}
#endif
#endif
