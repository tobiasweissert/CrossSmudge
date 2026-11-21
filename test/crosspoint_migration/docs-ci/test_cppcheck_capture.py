"""Test the real project diagnostic wrapper without launching any analyzer."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT/'scripts/ci_cppcheck_capture.py'

class AnalyzerCaptureTests(unittest.TestCase):
    def test_exact_command_include_list_header_and_preprocessor_retained(self):
        self.assertTrue(HELPER.exists(), 'missing cppcheck command-boundary capture')
        import sys
        sys.path.insert(0, str(ROOT/'scripts'))
        spec = importlib.util.spec_from_file_location('cpp_capture', HELPER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            root = Path(temp)
            (root/'lib/I18n').mkdir(parents=True)
            header = root/'lib/I18n/I18nKeys.h'
            header.write_bytes(b'exact generated fixture\n')
            includes = root/'temporary-includes'
            includes.write_text('/actual/includes\n')
            driver = root/'fake analyzer.py'
            driver.write_text('import sys; assert sys.argv[-1] == "-E"; print("preprocessed diagnostic fixture"); sys.exit(1)')
            command = [sys.executable, str(driver), '--language=c++', '--std=c++2b', '--includes-file='+str(includes), 'src/main.cpp']
            class Tool:
                def on_tool_output(self, line): pass
            tool = Tool()
            def execute(tool, args):
                self.assertEqual(args, command)
                self.assertTrue((root/'evidence/cppcheck-I18nKeys.h').is_file())
                header.write_bytes(b'changed after analyzer input was read\n')
                tool.on_tool_output('id=syntaxError')
                return {'returncode': 3}
            with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence')):
                old = Path.cwd()
                try:
                    os.chdir(root)
                    result = module.capture_execute(execute, tool, command)
                finally:
                    os.chdir(old)
            self.assertEqual(result, {'returncode': 3}, 'strict analyzer exit must survive diagnostics')
            self.assertEqual((root/'evidence/cppcheck-I18nKeys.h').read_bytes(), b'exact generated fixture\n')
            self.assertEqual((root/'evidence/cppcheck-0001-I18nKeys.h').read_bytes(), b'exact generated fixture\n')
            import json
            receipt = json.loads((root/'evidence/cppcheck-0001-receipt.json').read_text())
            self.assertFalse(receipt['preprocess_header_matches_input'])
            self.assertEqual((root/'evidence/cppcheck-0001-includes-0.txt').read_bytes(), includes.read_bytes())
            self.assertEqual((root/'evidence/cppcheck-0001-preprocess.log').read_bytes(), b'preprocessed diagnostic fixture\n')

    def test_descendants_cannot_write_after_capture(self):
        import json
        import sys
        import time
        sys.path.insert(0, str(ROOT/'scripts'))
        import ci_cppcheck_capture as module
        for mode in ('timeout', 'normal', 'closed-pipes', 'overflow', 'exception'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
                root = Path(temp)
                sentinel, pidfile = root/'late-write', root/'pid'
                driver = root/'fake analyzer.py'
                child = 'import pathlib,time;time.sleep(0.65);pathlib.Path('+repr(str(sentinel))+').write_text("late")'
                body = 'import os,sys,subprocess,pathlib,time\np=subprocess.Popen([sys.executable,"-c",'+repr(child)+'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)\npathlib.Path('+repr(str(pidfile))+').write_text(str(p.pid))\n'
                if mode == 'closed-pipes': body += 'os.close(1);os.close(2)\n'
                if mode == 'overflow': body += 'os.write(1,b"x"*300000)\n'
                if mode != 'normal': body += 'time.sleep(5)\n'
                driver.write_text(body)
                import selectors
                select = selectors.DefaultSelector.select
                def fault(selector, timeout=None):
                    if mode == 'exception' and pidfile.exists():
                        raise OSError('synthetic-monitor-failure')
                    return select(selector, timeout)
                class Tool:
                    def on_tool_output(self, line): pass
                def original(tool, command):
                    tool.on_tool_output('id=syntaxError')
                    return {'returncode': 3}
                with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence')), patch.object(module, 'PROBE_TIMEOUT', 0.25, create=True), patch.object(selectors.DefaultSelector, 'select', fault):
                    result = module.capture_execute(original, Tool(), [sys.executable, str(driver)])
                self.assertEqual(result, {'returncode': 3})
                receipt = json.loads((root/'evidence/cppcheck-0001-receipt.json').read_text())
                expected_error = {'timeout': 'TimeoutExpired', 'closed-pipes': 'TimeoutExpired',
                                  'overflow': 'OutputLimitExceeded', 'exception': 'OSError'}
                if mode in expected_error:
                    self.assertEqual(receipt.get('preprocess_error'), expected_error[mode])
                status = Path('/proc')/pidfile.read_text()/'stat'
                self.assertTrue(not status.exists() or status.read_text().split()[2] == 'Z', 'live analyzer descendant at callback return')
                time.sleep(0.8)
                self.assertFalse(sentinel.exists(), 'analyzer descendant wrote after callback')
                self.assertFalse(status.exists(), 'analyzer descendant not reaped by host init')

if __name__ == '__main__':
    unittest.main()
