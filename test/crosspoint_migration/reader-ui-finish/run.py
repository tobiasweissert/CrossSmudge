#!/usr/bin/env python3
"""Bounded extracted-production-body qualification; NOT firmware/visual acceptance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent

def text(path):
    return (ROOT / path).read_text()

def body(path, signature):
    source = text(path)
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    i = brace + 1
    while depth:
        if source[i] == '{': depth += 1
        elif source[i] == '}': depth -= 1
        i += 1
    return source[start:i]

def section(path, start, end):
    source = text(path)
    return source[source.index(start):source.index(end, source.index(start))]

def generate():
    reader = 'src/activities/reader/'
    settings = section('src/CrossPointSettings.h', 'enum class ReaderStatusBarPosition', 'struct LegacyReaderStatusBarSettings')
    chrome_h = section(reader+'XtcReaderMenuActivity.h', 'namespace ReaderChrome {', 'class XtcReaderMenuActivity')
    chrome = section(reader+'XtcReaderMenuActivity.cpp', 'namespace ReaderChrome {', 'namespace fui =')
    pre = (HERE/'chrome-double.h').read_text()
    chrome_unit = pre + settings + (HERE/'chrome-settings.h').read_text() + chrome_h + chrome
    consumer = '\n'.join(body(reader+'XtcReaderActivity.cpp', s) for s in [
        'XtcReaderActivity::StatusBarInfo XtcReaderActivity::getStatusBarInfo',
        'bool XtcReaderActivity::formatTimeLeftLabel',
        'void XtcReaderActivity::renderStatusBarOverlay'])
    shared_chrome = chrome_unit + (HERE/'xtc-double.h').read_text() + consumer
    chrome_unit = shared_chrome + (HERE/'chrome-tests.cpp').read_text()
    consumers = shared_chrome + (HERE/'epub-double.h').read_text()
    consumers += '\nconstexpr int PUBLISHER_PAGE_NUMBER_LEFT_MARGIN_MIN=12;\n'
    consumers += body(reader+'EpubReaderActivity.cpp', 'uint8_t effectiveReaderLeftMargin')
    consumers += section(reader+'EpubReaderActivity.cpp', 'struct ReaderViewportLayout', 'bool releaseReaderSdFontCachesForLowMemory')
    consumers += body(reader+'EpubReaderActivity.cpp', 'bool EpubReaderActivity::formatTimeLeftLabel')
    consumers += body(reader+'EpubReaderActivity.cpp', 'void EpubReaderActivity::renderStatusBar()')
    consumers += body(reader+'TxtReaderActivity.cpp', 'void TxtReaderActivity::renderStatusBar()')
    consumers += 'void EpubReaderActivity::handleTap(Touch touch) {' + body(reader+'EpubReaderActivity.cpp', 'if (touch.tapped && ReaderChrome::tapped') + '}\n'
    consumers += (HERE/'consumer-tests.cpp').read_text()
    stats_methods = '\n'.join(body(reader+'BookStatsActivity.cpp', s) for s in [
        'void BookStatsActivity::saveStats', 'void BookStatsActivity::beginDateEditing',
        'void BookStatsActivity::finishDateEditing', 'void BookStatsActivity::exitStatsActivity',
        'void BookStatsActivity::applyCompletedState'])
    stats = (HERE/'stats-double.h').read_text()+stats_methods+(HERE/'stats-tests.cpp').read_text()
    session = (HERE/'session-double.h').read_text()+body(reader+'ReaderActivity.cpp', 'void ReaderActivity::flushReaderSession')+body('src/activities/ActivityManager.cpp', 'void ActivityManager::setReaderSessionActive')+body('src/activities/ActivityManager.cpp', 'void ActivityManager::notifyInputLockChanged')+(HERE/'session-tests.cpp').read_text()
    actions = body(reader+'XtcReaderMenuActivity.h', 'enum class MenuAction')
    menu = pre + settings + (HERE/'chrome-settings.h').read_text()
    menu += (HERE/'menu-double.h').read_text().replace('__ACTIONS__', actions)
    menu += '\n'.join(body(reader+'XtcReaderMenuActivity.cpp', sig) for sig in [
        'std::vector<XtcReaderMenuActivity::MenuItem> XtcReaderMenuActivity::buildMenuItems',
        'void XtcReaderMenuActivity::refreshListItems', 'void XtcReaderMenuActivity::selectCurrent'])
    menu += body(reader+'XtcReaderActivity.cpp', 'void XtcReaderActivity::onReaderMenuConfirm').replace('void XtcReaderActivity::', 'void ReaderDispatch::', 1)
    menu += (HERE/'menu-tests.cpp').read_text()
    popup = (HERE/'session-double.h').read_text()+body(reader+'ReaderActivity.cpp', 'void ReaderActivity::flushReaderSession')+body('src/activities/ActivityManager.cpp', 'void ActivityManager::setReaderSessionActive')
    popup += '\n' + (HERE/'popup-double.h').read_text()
    popup += body(reader+'EpubReaderActivity.cpp', 'void EpubReaderActivity::openQuickActionsPopup')
    for format in ['Xtc', 'Txt']:
        popup += body(reader+format+'ReaderActivity.cpp', 'bool '+format+'ReaderActivity::handleShortcutAction(const CrossPointSettings::SHORT_PWRBTN action)')
    popup += (HERE/'popup-tests.cpp').read_text()
    return {'chrome': chrome_unit, 'consumers':consumers, 'menu':menu, 'stats':stats, 'session':session, 'popup':popup}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--evidence', required=True, type=Path)
    ap.add_argument('--sanitize', action='store_true')
    ap.add_argument('--only', choices=['chrome','consumers','menu','stats','session','popup'])
    args = ap.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    paths = ['src/activities/reader/'+f for f in ['EpubReaderActivity.cpp','EpubReaderActivity.h','XtcReaderActivity.cpp','XtcReaderActivity.h','TxtReaderActivity.cpp','TxtReaderActivity.h','XtcReaderMenuActivity.cpp','XtcReaderMenuActivity.h','BookStatsActivity.cpp']]+['src/activities/ActivityManager.cpp','src/activities/ActivityManager.h','src/activities/reader/ReaderSession.cpp','src/activities/reader/ReaderSession.h','src/CrossPointSettings.h']
    paths += ['src/activities/reader/ReaderActivity.cpp', 'src/activities/reader/BookStatsView.cpp', 'src/activities/reader/BookStatsView.h']
    paths += [str(p.relative_to(ROOT)) for p in sorted(HERE.iterdir()) if p.is_file()]
    hashes = {p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}
    results=[]
    for name, code in generate().items():
        if args.only and args.only != name: continue
        cpp=args.evidence/(name+'.cpp'); exe=args.evidence/name
        cpp.write_text(code)
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-I',str(ROOT/'src/activities/reader'),str(cpp)]
        if name in ('session','popup'): cmd += [str(ROOT/'src/activities/reader/ReaderSession.cpp')]
        cmd += ['-o',str(exe)]
        if args.sanitize: cmd += ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie']
        for phase, argv in [('compile',cmd),('run',[str(exe)])]:
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
            try:
                proc=subprocess.run(argv,capture_output=True,text=True,timeout=90,env=env)
                rc=proc.returncode; out=proc.stdout+proc.stderr
            except subprocess.TimeoutExpired as exc:
                rc=124;out=str(exc)
            receipt={'suite':name,'phase':phase,'argv':argv,'returncode':rc,'output':out,'source_hashes':hashes,'generated_sha256':hashlib.sha256(code.encode()).hexdigest()}
            (args.evidence/(name+'-'+phase+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
            print(name,phase,rc,out,flush=True)
            if rc: raise SystemExit(rc)
        results.append(name)
    assert hashes == {p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}, 'source changed during qualification'
    (args.evidence/'summary.json').write_text(json.dumps({'suites':results,'sanitize':args.sanitize,'all_passed':True,'hashes':hashes},indent=2)+'\n')

if __name__=='__main__': main()
