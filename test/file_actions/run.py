#!/usr/bin/env python3
"""Execute unchanged production method bodies over isolated host boundaries.
No SDK/target build or physical SD semantics are simulated as device proof.
"""
from pathlib import Path
import fcntl
import os
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent

def method(path, signature):
    text = (ROOT / path).read_text()
    start = text.index(signature)
    # Production methods end at an unindented brace (including inner lambdas).
    end = text.index('\n}', start) + 2
    return text[start:end]

def run():
    import re
    browser = (ROOT / 'src/activities/home/FileBrowserActivity.cpp').read_text()
    ids = sorted(set(re.findall(r'\bSTR_[A-Z0-9_]+', browser)))
    action_header = (ROOT / 'src/activities/home/FileBrowserActionActivity.h').read_text()
    enums = action_header[action_header.index('enum class FileBrowserAction'):action_header.index('\n};')+3]
    source = 'enum class StrId {' + ','.join(ids) + '};\n'
    source += '\n'.join('constexpr auto '+i+'=StrId::'+i+';' for i in ids) + '\n' + enums + '\n'
    gate = '\n'.join(line for line in (ROOT/'src/activities/home/FileMutationInputGate.h').read_text().splitlines() if not line.startswith('#'))
    source += (HERE / 'boundaries.h').read_text().replace('// INPUT_GATE',gate)
    source += (HERE / 'migration_boundaries.h').read_text()
    source += '\n' + '\n'.join(line for line in (ROOT/'src/util/BookMoveUtils.h').read_text().splitlines() if not line.startswith('#'))
    source += '\nnamespace {\n' + method('src/util/BookMoveUtils.cpp', 'bool getCachePath(') + '\n}\nnamespace BookMoveUtils {\n'
    source += method('src/util/BookMoveUtils.cpp', 'RenameMigrationResult migrateRenamedBookState(') + '\n}\n'
    source += '\nnamespace BookActions {\n' + method('src/activities/home/BookActions.cpp','bool clearFileMetadata(') + '\n}\n'
    source += '\n' + '\n'.join(line for line in (ROOT/'src/activities/home/FileActionPaths.h').read_text().splitlines() if not line.startswith('#'))
    source += '\n' + method('src/activities/home/FileBrowserActivity.cpp', 'void FileBrowserActivity::promptDeleteFile(')
    source += '\n' + method('src/activities/home/FileBrowserActivity.cpp', 'void FileBrowserActivity::showFileActionMenu(')
    source += '\n' + method('src/activities/home/FileBrowserActivity.cpp', 'void FileBrowserActivity::startMoveFile(')
    source += '\n' + method('src/activities/home/FileBrowserActivity.cpp', 'void FileBrowserActivity::moveFile(')
    source += '\n' + method('src/activities/home/FileBrowserActivity.cpp', 'void FileBrowserActivity::fileOperationError(')
    source += '\n' + method('src/activities/home/FileBrowserActivity.cpp', 'void FileBrowserActivity::buildListScreen(')
    source += '\n' + method('src/activities/home/FileBrowserActivity.cpp', 'void FileBrowserActivity::onRowEvent(')
    for name in ['loop','render','navigateBack','activateSelected','selectCurrentFolder','onFileControlEvent','showDirectoryActionMenu']:
        source += '\n' + method('src/activities/home/FileBrowserActivity.cpp','void FileBrowserActivity::'+name+'(')
    registrations=browser[browser.index('  app.on(ACTION_ROW'):browser.index('  app.setScreen(',browser.index('  app.on(ACTION_ROW'))]
    source += '\nvoid FileBrowserActivity::registerActions(){\n'+registrations+'\n}\n'
    source += '\n' + (HERE / 'behavior.cpp').read_text()
    with open(Path(tempfile.gettempdir())/'crosscub-review-probes.lock', 'a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print('BLOCKED: shared compiler lock busy', file=sys.stderr)
            return 75
        with tempfile.TemporaryDirectory(prefix='crosscub-files-') as tmp:
            cpp = Path(tmp) / 'probe.cpp'
            cpp.write_text(source)
            flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', *(['-fno-pie', '-no-pie'] if sys.platform != 'darwin' else [])] if '--sanitize' in sys.argv else []
            cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-const-variable', '-O1', '-g', *flags, '-I'+str(HERE),str(cpp), '-o', str(Path(tmp) / 'probe')]
            print('COMMAND:', ' '.join(cmd), flush=True)
            subprocess.run(cmd, check=True, timeout=90)
            first = subprocess.run([str(Path(tmp) / 'probe')], timeout=30).returncode
            if first:
                return first
            header=ROOT/'src/activities/home/FileMutationConfirmationActivity.h'
            subject=('#include "activities/home/FileMutationConfirmationActivity.h"\n' if header.exists() else
                     '#include "activities/util/ConfirmationActivity.h"\nusing FileMutationConfirmationActivity=ConfirmationActivity;\n')
            (Path(tmp)/'Subject.h').write_text(subject)
            includes=['-I'+str(HERE/'stubs'),'-I'+str(ROOT/'test/option_popup/stubs'),'-I'+str(ROOT/'src'),'-I'+tmp]
            cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g',*flags,*includes,
                 '-DCROSSINK_APP_CAP_TOUCH=1',str(HERE/'confirmation.cpp'),
                 str(ROOT/'src/activities/util/ConfirmationActivity.cpp'),'-o',tmp+'/confirmation']
            print('COMMAND:', ' '.join(cmd),flush=True)
            subprocess.run(cmd,check=True,timeout=90)
            return subprocess.run([tmp+'/confirmation'],timeout=30).returncode

if __name__ == '__main__':
    sys.exit(run())
