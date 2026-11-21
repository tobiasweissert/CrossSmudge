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
    def run_entrypoint(self, root, behavior, capture='none'):
        # Only PlatformIO is controlled: execute the actual production __main__
        # in a fresh OS process, including its real diagnostic capture callback.
        import subprocess
        import sys
        bootstrap = '''
import json, pathlib, runpy, sys, types
scripts, behavior, capture = sys.argv[1:]
sys.path.insert(0, scripts)
for name in ('platformio', 'platformio.check', 'platformio.check.tools',
             'platformio.check.tools.cppcheck', 'platformio.__main__'):
    sys.modules[name] = types.ModuleType(name)
command = [sys.executable, '-c',
           'import sys; sys.stderr.write("private-diagnostic-canary\\\\n"); sys.exit(7)']
if capture == 'error':
    command = ['/nonexistent/private-diagnostic-canary']
strict_result = {'returncode': 3, 'stdout': 'unchanged strict result',
                 'stderr': 'unchanged strict error'}
class Tool:
    def on_tool_output(self, line):
        pass
    def execute_check_cmd(self, args):
        assert args == command
        if capture != 'plain':
            self.on_tool_output('id=syntaxError')
        return strict_result
sys.modules['platformio.check.tools.cppcheck'].CppcheckCheckTool = Tool
def main():
    assert sys.argv[1:] == ['check', '--verbose', '--fail-on-defect', 'low',
                            '--fail-on-defect', 'medium', '--fail-on-defect', 'high']
    if capture != 'none':
        tool = Tool()
        output = tool.on_tool_output
        result = tool.execute_check_cmd(command)
        assert result is strict_result, 'capture replaced the strict result'
        assert tool.on_tool_output == output, 'capture did not restore output'
        pathlib.Path('observed-result.json').write_text(json.dumps(result))
    if behavior == 'raise3':
        raise SystemExit(3)
    if behavior == 'raise0':
        raise SystemExit(0)
    return None if behavior == 'None' else int(behavior)
sys.modules['platformio.__main__'].main = main
sys.argv = [str(pathlib.Path(scripts)/'ci_cppcheck_capture.py'), 'check',
            '--verbose', '--fail-on-defect', 'low', '--fail-on-defect',
            'medium', '--fail-on-defect', 'high']
runpy.run_path(sys.argv[0], run_name='__main__')
'''
        env = {'PATH': os.defpath, 'CROSSCUB_PRIVATE_EVIDENCE': str(root/'evidence')}
        return subprocess.run([sys.executable, '-I', '-B', '-c', bootstrap,
                               str(ROOT/'scripts'), behavior, capture],
                              cwd=root, env=env, capture_output=True, timeout=10)

    def test_actual_entrypoint_propagates_core_nonzero_return(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            result = self.run_entrypoint(Path(temp), '3')
        self.assertEqual(result.returncode, 3, 'Core returned 3 but wrapper reported OS success')
        self.assertEqual(result.stdout, b'')
        self.assertEqual(result.stderr, b'')

    def test_actual_entrypoint_preserves_success_and_system_exit(self):
        for behavior, expected in (('0', 0), ('None', 0), ('raise0', 0), ('raise3', 3)):
            with self.subTest(behavior=behavior), tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
                result = self.run_entrypoint(Path(temp), behavior)
                self.assertEqual(result.returncode, expected)
                self.assertEqual(result.stdout, b'')
                self.assertEqual(result.stderr, b'')

    def test_actual_entrypoint_preserves_capture_results_and_private_errors(self):
        import json
        import stat
        for capture in ('plain', 'syntax', 'error'):
            for behavior in ('3', 'raise3'):
                with self.subTest(capture=capture, behavior=behavior), tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
                    root = Path(temp)
                    (root/'lib/I18n').mkdir(parents=True)
                    (root/'lib/I18n/I18nKeys.h').write_bytes(b'private header fixture\n')
                    result = self.run_entrypoint(root, behavior, capture)
                    self.assertEqual(result.returncode, 3)
                    self.assertEqual(result.stdout, b'')
                    self.assertEqual(result.stderr, b'')
                    self.assertEqual(json.loads((root/'observed-result.json').read_text()),
                                     {'returncode': 3, 'stdout': 'unchanged strict result',
                                      'stderr': 'unchanged strict error'})
                    evidence = root/'evidence'
                    self.assertEqual(stat.S_IMODE(evidence.stat().st_mode), 0o700)
                    for file in evidence.iterdir():
                        self.assertEqual(stat.S_IMODE(file.stat().st_mode), 0o600)
                    receipt_path = evidence/'cppcheck-0001-receipt.json'
                    if capture == 'plain':
                        self.assertFalse(receipt_path.exists())
                        continue
                    receipt = json.loads(receipt_path.read_text())
                    self.assertEqual(receipt['original_exit'], 3)
                    self.assertFalse(receipt['publication_authorized'])
                    if capture == 'syntax':
                        self.assertEqual(receipt['preprocess_exit'], 7)
                        self.assertNotIn('preprocess_error', receipt)
                        self.assertEqual((evidence/'cppcheck-0001-preprocess.log').read_bytes(),
                                         b'private-diagnostic-canary\n')
                    else:
                        self.assertEqual(receipt['preprocess_error'], 'FileNotFoundError')
                        self.assertNotIn('preprocess_exit', receipt)

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
