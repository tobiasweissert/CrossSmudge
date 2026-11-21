#!/usr/bin/env python3
"""Run unchanged strict PIO check, preserving failing analyzer inputs privately."""
import os
from pathlib import Path
import subprocess
import sys

from ci_producer_capture import output_root, retain, save_receipt, bounded_diagnostic

PROBE_TIMEOUT = 60


def capture_execute(original, tool, command):
    out = output_root()
    if not (out/'cppcheck-generated-header-receipt.json').exists():
        files = {}
        retain(out, 'cppcheck-I18nKeys.h', Path.cwd()/'lib/I18n/I18nKeys.h', files)
        save_receipt(out, 'cppcheck-generated-header-receipt.json', {'files': files, 'publication_authorized': False})
    from capture_sticky_startup_config import read_regular
    header_path = Path.cwd()/'lib/I18n/I18nKeys.h'
    header_data = None
    header_error = None
    try:
        header_data = read_regular(header_path)
    except (OSError, ValueError) as exc:
        header_error = type(exc).__name__
    saw_syntax = []
    output = tool.on_tool_output
    def observe(line):
        if 'id=syntaxError' in line:
            saw_syntax.append(True)
        return output(line)
    tool.on_tool_output = observe
    try:
        result = original(tool, command)
    finally:
        tool.on_tool_output = output
    if not saw_syntax:
        return result
    index = getattr(tool, '_crosscub_syntax_count', 0) + 1
    tool._crosscub_syntax_count = index
    # Bound extra analyzer work; original strict check runs unchanged for all TUs.
    if index > 3:
        return result
    prefix = 'cppcheck-%04d' % index
    receipt = {'command': command, 'original_exit': result['returncode'], 'files': {}, 'publication_authorized': False}
    if header_data is not None:
        import hashlib
        name = prefix+'-I18nKeys.h'
        with (out/name).open('xb') as stream:
            os.chmod(out/name, 0o600)
            stream.write(header_data)
        receipt['files'][name] = {'source': str(header_path), 'present': True, 'sha256': hashlib.sha256(header_data).hexdigest(), 'boundary': 'before-exact-analyzer-command'}
        try:
            receipt['preprocess_header_matches_input'] = read_regular(header_path) == header_data
        except (OSError, ValueError):
            receipt['preprocess_header_matches_input'] = False
    else:
        receipt['header_error'] = header_error
    for i, arg in enumerate(a for a in command if a.startswith('--includes-file=')):
        retain(out, prefix+'-includes-'+str(i)+'.txt', Path(arg.split('=', 1)[1].strip('"')), receipt['files'])
    # Core 6.1.19 proc.exec_command passes argv directly to subprocess.Popen.
    # Preserve even its quoted template/source tokens exactly; append only -E.
    args = list(command) + ['-E']
    receipt['preprocess_command'] = args
    try:
        receipt['preprocess_exit'] = bounded_diagnostic(args, out/(prefix+'-preprocess.log'), timeout=PROBE_TIMEOUT)
    except Exception as exc:
        # Only diagnostic errors are contained; original strict result survives.
        receipt['preprocess_error'] = type(exc).__name__
    save_receipt(out, prefix+'-receipt.json', receipt)
    return result


def main():
    from platformio.check.tools.cppcheck import CppcheckCheckTool
    from platformio.__main__ import main as pio_main
    original = CppcheckCheckTool.execute_check_cmd
    def execute(tool, command):
        return capture_execute(original, tool, command)
    CppcheckCheckTool.execute_check_cmd = execute
    # No changed check options, severities, suppressions or result handling.
    pio_main()


if __name__ == '__main__':
    main()
