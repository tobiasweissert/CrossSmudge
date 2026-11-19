#!/usr/bin/env python3
"""Actual ActivityResult header with independent reader-model type doubles."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
def main():
    with tempfile.TemporaryDirectory(prefix='sdk-ui-results-',dir=os.environ['TMPDIR']) as td:
        d=Path(td)
        (d/'reader').mkdir();(d/'util').mkdir()
        (d/'ActivityResult.h').write_bytes((ROOT/'src/activities/ActivityResult.h').read_bytes())
        (d/'reader/EpubReaderMenuModel.h').write_text('#pragma once\nstruct ReaderDrawerState{};enum class ReaderSettingsChangeMask{None};\n')
        (d/'util/FrontlightPanelModel.h').write_text('#pragma once\nstruct FrontlightPanelResult{};\n')
        (d/'main.cpp').write_text(r'''
#include "ActivityResult.h"
#include <cassert>
int main(){
 MenuResult menu{4,2,true};assert(menu.action==4&&menu.orientation==2&&menu.settingsChanged&&menu.pageTurnOption==0);
 ClippingResult clip{"a",1,2,3,4,5,6,7,8};
 assert(clip.text=="a"&&clip.sectionPage==1&&clip.wordCount==8&&clip.action==ClippingResult::Action::Clip);
 clip.action=ClippingResult::Action::Lookup;clip.startOffset=123;
 ActivityResult result{clip};assert(std::get<ClippingResult>(result.data).startOffset==123);
 ActivityResult progress{ProgressChangeResult{.spineIndex=1,.page=2,.totalPages=3}};
 assert(std::get<ProgressChangeResult>(progress.data).page==2);
 ActivityResult percent{PercentResult{12.25f}};assert(std::get<PercentResult>(percent.data).percent==12.25f);
 ActivityResult dictionary{DictionaryClippingRequest{}};assert(std::holds_alternative<DictionaryClippingRequest>(dictionary.data));
}
''')
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-fno-exceptions','-fno-rtti','-I'+td,str(d/'main.cpp'),'-o',str(d/'probe')]
        if os.environ.get('SANITIZE')=='1':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);subprocess.run([str(d/'probe')],check=True)
        print('PASS: actual ActivityResult header, owner aggregate prefix/decimal precision/variants and additive upstream progress/clipping fields')
if __name__=='__main__':main()
