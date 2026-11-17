"""Bounded production-seam regressions; inherited by the routine Product suite.
No firmware build, network, flash, SDK writes or dependencies installed.
"""
from pathlib import Path
import configparser
import hashlib
import json
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SANITIZE = (['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
            if os.environ.get('CROSSCUB_TEST_SANITIZE') == '1' else [])


def compile_run(source, extra=(), args=()):
    with tempfile.TemporaryDirectory(prefix='crosscub-round1-') as tmp:
        cpp = Path(tmp) / 'probe.cpp'
        cpp.write_text(source)
        command = ['g++', '-std=c++17', *SANITIZE, '-I'+str(ROOT),
                   '-I'+str(ROOT/'lib/JsonParser'), str(cpp), *map(str, extra), '-o', tmp+'/probe']
        built = subprocess.run(command, capture_output=True, text=True)
        if built.returncode:
            raise AssertionError(built.stderr)
        ran = subprocess.run([tmp+'/probe', *args], capture_output=True, text=True)
        if ran.returncode:
            raise AssertionError(ran.stderr)


def ota_fragments():
    source = (ROOT/'src/network/OtaUpdater.cpp').read_text()
    comparator = source[source.index('struct ParsedVersion'):source.index('bool startsWith(')]
    helpers = source[source.index('char lowerHex('):source.index('bool isHttpUrl(')]
    matcher = '\n'.join(re.search(r'bool '+fn+r'\([^\n]*\) \{.*?\n\}', source, re.S).group()
                        for fn in ('startsWith', 'endsWith', 'isMatchingFirmwareAssetName'))
    guard = source[source.index('  uint8_t descriptor[112];'):source.index('  const auto validationResult')]
    return comparator, helpers, matcher, guard


class Round1(unittest.TestCase):
    def test_wire_release_to_version_bound_image_admission(self):
        comparator, helpers, matcher, guard = ota_fragments()
        # Execute the actual pre-flash descriptor guard with a bounded read-only
        # file adapter. Extract unchanged comparator and asset matcher verbatim.
        probe = '''#include "src/network/CrosscubOtaPolicy.h"
#include "ReleaseJsonParser.h"
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstring>
#include <algorithm>
#define CROSSSMUDGE_VERSION "1.6.5.3-sticky"
#define LOG_ERR(...) ((void)0)
constexpr size_t VERSION_SEGMENT_COUNT=4;
constexpr char firmwareAssetStem[]="crosscub-sticky";
constexpr char firmwareAssetName[]="crosscub-sticky.bin";
constexpr char binSuffix[]=".bin";
''' + comparator + helpers + matcher + '''
struct File {
 const std::vector<uint8_t>& data;
 bool seek(int){return true;}
 size_t read(uint8_t* out,size_t n){n=std::min(n,data.size());memcpy(out,data.data(),n);return n;}
 void close(){}
};
struct {void remove(const char*){}} Storage;
constexpr const char* OTA_STAGE_PATH="unused";
constexpr bool WRONG_DEVICE_ERROR=false;
bool admission(const std::vector<uint8_t>& bytes,const std::string& latestVersion){
 File stagedFile{bytes};
''' + guard + '''
 return true;
}
int main(int argc,char**argv){
 assert(argc==5);
 std::ifstream image(argv[1],std::ios::binary), wire(argv[2]);
 std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(image)),{});
 std::string json((std::istreambuf_iterator<char>(wire)),{});
 ReleaseJsonParser parser(isMatchingFirmwareAssetName);
 for(size_t i=0;i<json.size();i+=7)parser.feed(json.data()+i,std::min(size_t(7),json.size()-i));
 bool accepted=parser.foundTag() && parser.foundFirmware();
 accepted=accepted && crosscub_ota::acceptsDownload(parser.getTagName(),parser.getFirmwareUrl(),parser.getFirmwareSha256(),parser.getFirmwareSize());
 uint8_t actual[32];
 for(size_t i=0;i<32;++i){unsigned n=0;sscanf(argv[3]+2*i,"%2x",&n);actual[i]=n;}
 accepted=accepted && bytes.size()==parser.getFirmwareSize() && sha256Matches(actual,parser.getFirmwareSha256());
 accepted=accepted && compareVersions(crosscub_ota::version(parser.getTagName()),CROSSSMUDGE_VERSION)>0;
 accepted=accepted && admission(bytes,parser.getTagName());
 return accepted==(std::string(argv[4])=="1")?0:10;
}
'''
        cases = [
            ('matching newer', '1.6.5.4', '1.6.5.4', True, {}),
            ('mislabeled old', '9.0.0', '1.0.0', False, {}),
            ('different newer', '9.0.0', '8.0.0', False, {}),
            ('matching older', '1.0.0', '1.0.0', False, {}),
            ('matching equal', '1.6.5.3', '1.6.5.3', False, {}),
            ('empty', '9.0.0', '', False, {}),
            ('unterminated', '9.0.0', '9'*32, False, {}),
            ('malformed', '9.0.0-', '9.0.0-', False, {}),
            ('overflow', '9999999999', '9999999999', False, {}),
            ('bad project', '9.0.0', '9.0.0', False, {'project':'CrossSmudge'}),
            ('bad digest', '9.0.0', '9.0.0', False, {'digest':'0'*64}),
            ('bad size', '9.0.0', '9.0.0', False, {'size':65537}),
            ('bad owner', '9.0.0', '9.0.0', False, {'owner':'Mumfee'}),
            ('bad asset', '9.0.0', '9.0.0', False, {'name':'firmware-sticky.bin'}),
        ]
        # Compile once; every invocation feeds the real parser and same guard.
        with tempfile.TemporaryDirectory(prefix='crosscub-wire-') as tmp:
            tmp=Path(tmp); cpp=tmp/'probe.cpp';cpp.write_text(probe)
            subprocess.run(['g++','-std=c++17',*SANITIZE,'-I'+str(ROOT),'-I'+str(ROOT/'lib/JsonParser'),str(cpp),
                            str(ROOT/'lib/JsonParser/ReleaseJsonParser.cpp'),
                            str(ROOT/'lib/JsonParser/StreamingJsonParser.cpp'),'-o',str(tmp/'probe')],check=True)
            for label, tag_version, embedded, expected, overrides in cases:
                with self.subTest(label=label):
                    data=bytearray(65536);data[0]=0xe9;data[32:36]=bytes.fromhex('3254cdab')
                    raw=embedded.encode();data[48:48+len(raw)]=raw
                    project=overrides.get('project','Crosscub').encode();data[80:80+len(project)]=project
                    digest=hashlib.sha256(data).hexdigest();tag='crosscub-v'+tag_version
                    asset={'name':overrides.get('name','crosscub-sticky-v'+tag_version+'.bin'),
                           'browser_download_url':'https://github.com/'+overrides.get('owner','tobiasweissert')+'/CrossSmudge/releases/download/'+tag+'/crosscub-sticky-v'+tag_version+'.bin',
                           'size':overrides.get('size',len(data)), 'digest':'sha256:'+overrides.get('digest',digest)}
                    (tmp/'image').write_bytes(data);(tmp/'wire').write_text(json.dumps({'tag_name':tag,'assets':[asset]}))
                    result=subprocess.run([str(tmp/'probe'),str(tmp/'image'),str(tmp/'wire'),digest,str(int(expected))])
                    self.assertEqual(result.returncode,0,label+' admission differs from contract')

    def test_all_sdk_profile_flags_rejected_at_both_boundaries(self):
        sdk=(ROOT/'freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h').read_text()
        selection=sdk[sdk.index('#if FREEINK_DEVICE_WS397\nconstexpr BoardProfile DEFAULT_DEVICE'):sdk.index('// Runtime-active profile.')]
        flags=sorted(set(re.findall(r'FREEINK_DEVICE_\w+',selection)))
        self.assertEqual(set(flags),set(re.findall(r'#ifndef (FREEINK_DEVICE_\w+)',sdk)))
        profiles=sorted(set(re.findall(r'DEFAULT_DEVICE = (\w+)',selection)))
        # Stub only hardware dependencies, not profile selection: run the pinned
        # SDK's actual selection block, including precedence and X3/X4 fallback.
        stub='#pragma once\n#define FREEINK_CAP_TOUCH 1\n#define FREEINK_CAP_USB_MSC 0\nusing BoardProfile=int;\n'
        stub+='enum {'+','.join(profiles)+'};\n'+selection
        with tempfile.TemporaryDirectory(prefix='crosscub-exclusive-') as tmp:
            tmp=Path(tmp);(tmp/'BoardConfig.h').write_text(stub)
            for boundary in ['AppCapabilities.h','src/network/FirmwareBoardTag.cpp']:
                cpp=tmp/'board.cpp'
                cpp.write_text('#include "'+boundary+'"\nstatic_assert(DEFAULT_DEVICE==STICKY);\nint main(){}\n')
                cmd=['g++','-std=c++17','-fsyntax-only','-I'+str(tmp),'-I'+str(ROOT),'-I'+str(ROOT/'include'),
                     '-DARDUINO_ARCH_ESP32=1','-DCROSSSMUDGE_APP_CAP_TOUCH=1','-DCROSSINK_APP_CAP_USB_DRIVE=0',str(cpp)]
                for flag in flags:
                    if flag=='FREEINK_DEVICE_STICKY':continue
                    with self.subTest(boundary=boundary,flag=flag):
                        # Compile without the test-only selected-profile assertion:
                        # app admission itself must reject even lower-precedence profiles.
                        cpp.write_text('#include "'+boundary+'"\nint main(){}\n')
                        bad=subprocess.run(cmd+['-DFREEINK_DEVICE_STICKY=1','-D'+flag+'=1'],capture_output=True)
                        self.assertNotEqual(bad.returncode,0,flag+' accepted at '+boundary)
                cpp.write_text('#include "'+boundary+'"\nstatic_assert(DEFAULT_DEVICE==STICKY);\nint main(){}\n')
                good=subprocess.run(cmd+['-DFREEINK_DEVICE_STICKY=1'],capture_output=True)
                self.assertEqual(good.returncode,0,good.stderr.decode())
            config=configparser.ConfigParser(interpolation=None)
            config.read(ROOT/'platformio.ini')
            def value(section, key):
                raw=config.get(section,key,fallback=None)
                if raw is None:
                    return value(config.get(section,'extends'),key)
                return re.sub(r'\$\{([^}]+)\}',lambda m:value(*m[1].rsplit('.',1)),raw)
            for name in ['sticky','sticky-debug','sticky-owner-test','default','debug','sticky-simulator','simulator']:
                with self.subTest(profile=name):
                    macros=re.findall(r'-D([A-Z_0-9]+)(?:=([^\s]+))?',value('env:'+name,'build_flags'))
                    defs=['-D'+key+('='+v if v else '') for key,v in macros
                          if key.startswith('FREEINK_DEVICE_') or key in ['SIMULATOR','CROSSSMUDGE_APP_CAP_TOUCH','CROSSINK_APP_CAP_USB_DRIVE']]
                    if 'simulator' not in name:defs+=['-DARDUINO_ARCH_ESP32=1']
                    cpp.write_text('#include "AppCapabilities.h"\n'+('static_assert(DEFAULT_DEVICE==STICKY);\n' if 'simulator' not in name else '')+'int main(){}\n')
                    check=subprocess.run(['g++','-std=c++17','-fsyntax-only','-I'+str(tmp),'-I'+str(ROOT/'include'),str(cpp),*defs],capture_output=True)
                    self.assertEqual(check.returncode,0,check.stderr.decode())

    def test_http_configuration_keeps_pinned_idf_order_and_security(self):
        source=(ROOT/'src/network/OtaUpdater.cpp').read_text()
        config=source[source.index('  esp_http_client_config_t client_config'):source.index('  totalBytesReceived = 0;',source.index('OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate'))]
        # Interface-compatible slice of pinned esp32s3 IDF config (declaration
        # order: redirect precedes handler; bundle follows user_data).
        compile_run('''#include <cassert>
struct esp_http_client_config_t {
 const char* url; bool disable_auto_redirect; int event_handler;
 int buffer_size; int buffer_size_tx; void* user_data;
 int crt_bundle_attach; bool keep_alive_enable;
};
int main(){const char* latestReleaseUrl="owner";int releaseParser;
int release_manifest_event_handler=1,esp_crt_bundle_attach=2;
'''+config+'''
assert(client_config.url==latestReleaseUrl && client_config.disable_auto_redirect);
assert(client_config.event_handler==1 && client_config.buffer_size==4096 && client_config.buffer_size_tx==1024);
assert(client_config.user_data==&releaseParser && client_config.crt_bundle_attach==2 && client_config.keep_alive_enable);
}''')

    def test_readme_separates_preparation_from_live_publication(self):
        text=(ROOT/'README.md').read_text()
        self.assertNotIn('Instant Distribution',text)
        self.assertIn('integration/sticky-test-all',text)
        self.assertIn('41',text)
        self.assertIn('explicitly authorized',text)

    def test_readme_matches_production_heap_and_license(self):
        text=(ROOT/'README.md').read_text()
        self.assertNotIn('Mandatory Hardware Constraint Testing',text)
        self.assertNotIn('exact hardware limits',text)
        self.assertIn('2 MB',text)
        self.assertIn('4 MB',text)
        self.assertIn('optional stress',text)
        self.assertIn('test/crosscub/test_product.py',text)
        self.assertIn('MIT',text)
        self.assertNotIn('licensed under the **GNU General Public License',text)
