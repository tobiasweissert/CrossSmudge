#pragma once
enum class StrId {STR_BACK,STR_CANCEL,STR_SAVE,STR_SELECT,STR_DIR_UP,STR_DIR_DOWN,STR_CONFIRM};
constexpr auto STR_BACK=StrId::STR_BACK,STR_CANCEL=StrId::STR_CANCEL,STR_SAVE=StrId::STR_SAVE,STR_SELECT=StrId::STR_SELECT,STR_DIR_UP=StrId::STR_DIR_UP,STR_DIR_DOWN=StrId::STR_DIR_DOWN;
struct I18nStub{const char*get(StrId)const{return "translated";}};
inline I18nStub I18N;
inline const char*tr(StrId){return "translated";}
