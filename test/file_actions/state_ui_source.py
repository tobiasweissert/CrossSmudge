"""Production Delete/Move bodies; UI HAL fixture bridges saves to real CrossPointState."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
def method(text,signature):
 start=text.index(signature)
 return text[start:text.index('\n}',start)+2]
def source(browser_path=None):
 browser=Path(browser_path or ROOT/'src/activities/home/FileBrowserActivity.cpp').read_text()
 ids=sorted(set(re.findall(r'\bSTR_[A-Z0-9_]+',browser)))
 header=(ROOT/'src/activities/home/FileBrowserActionActivity.h').read_text()
 enums=header[header.index('enum class FileBrowserAction'):header.index('\n};')+3]
 boundaries=(HERE/'boundaries.h').read_text()
 includes='\n'.join(l for l in boundaries.splitlines() if l.startswith('#include') and 'ui_boundaries' not in l)
 boundaries='\n'.join(l for l in boundaries.splitlines() if not (l.startswith('#include') and 'ui_boundaries' not in l))
 boundaries=boundaries.replace('return saveOk;', 'return saveOk && ::persistReferences(openEpubPath, favoriteSleepImagePath, favoriteBootImagePath);')
 gate='\n'.join(l for l in (ROOT/'src/activities/home/FileMutationInputGate.h').read_text().splitlines() if not l.startswith('#'))
 source=includes+'\n#include <CrossPointState.h>\n#include <HalStorage.h>\n'
 source+='bool persistReferences(const std::string&o,const std::string&s,const std::string&b){auto&state=APP_STATE;state.openEpubPath=o;state.favoriteSleepImagePath=s;state.favoriteBootImagePath=b;return state.saveToFile();}\n#undef APP_STATE\n'
 source+='namespace files_fixture {\nenum class StrId {'+','.join(ids)+'};\n'
 source+='\n'.join('[[maybe_unused]] constexpr auto '+i+'=StrId::'+i+';' for i in ids)+'\n'+enums+'\n'
 source+=boundaries.replace('// INPUT_GATE',gate)+'\n'
 source+='namespace BookActions {bool clearFileMetadata(const std::string&){return true;}}\n'
 source+='namespace BookMoveUtils {enum class RenameMigrationResult{Success,RolledBack,Uncertain};template<class...T>RenameMigrationResult migrateRenamedBookState(T&&...){return RenameMigrationResult::Success;}}\n'
 source+='\n'.join(l for l in (ROOT/'src/activities/home/FileActionPaths.h').read_text().splitlines() if not l.startswith('#'))+'\n'
 for name in ['promptDeleteFile','moveFile','fileOperationError']:
  source+=method(browser,'void FileBrowserActivity::'+name+'(')+'\n'
 source+='void FileBrowserActivity::loop(){}\nvoid FileBrowserActivity::render(RenderLock&&){}\n}\n'
 source+=(HERE/'state_ui.cpp').read_text()
 return source
