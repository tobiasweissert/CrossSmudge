#!/usr/bin/env python3
"""Execute the production OTA activity's check-result state transition."""
from pathlib import Path
import re, subprocess,tempfile
R=Path(__file__).resolve().parents[2]
s=(R/'src/activities/settings/OtaUpdateActivity.cpp').read_text()
body=re.search(r'void OtaUpdateActivity::onWifiSelectionComplete\(const bool success\) \{.*?\n\}',s,re.S).group()
prefix='''#include <cassert>
#define LOG_ERR(...) ((void)0)
#define LOG_DBG(...) ((void)0)
struct OtaUpdater {enum {OK,NO_UPDATE}; int result=NO_UPDATE; int checkForUpdate(){return result;}bool isUpdateNewer(){return false;}};
int failureMessageFor(int){return 9;}
struct OtaUpdateActivity {
 enum State{CHECKING_FOR_UPDATE,FAILED,NO_UPDATE,WAITING_CONFIRMATION}; State state=WAITING_CONFIRMATION;
 enum class RequestUpdateResult {Rendered,Other};
 OtaUpdater updater;int failureMessage=0;
 void finish(){}void requestUpdate(bool){}RequestUpdateResult requestUpdateAndWait(){return RequestUpdateResult::Rendered;}
 void onWifiSelectionComplete(bool);
};
struct RenderLock {RenderLock(OtaUpdateActivity&){};};
'''
with tempfile.TemporaryDirectory(prefix='crosscub-ota-state-') as tmp:
 p=Path(tmp);(p/'state.cpp').write_text(prefix+body+'\nint main(){OtaUpdateActivity a;a.onWifiSelectionComplete(true);assert(a.state==OtaUpdateActivity::NO_UPDATE);}\n')
 subprocess.run(['g++',str(p/'state.cpp'),'-o',str(p/'state')],check=True)
 subprocess.run([str(p/'state')],check=True)
print('PASS: unpublished product channel is a no-update state, not a failed install')
