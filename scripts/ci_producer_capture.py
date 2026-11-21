#!/usr/bin/env python3
"""Opt-in CI producer evidence. Raw bytes must stay in the CMS retention lane."""
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

PROBE_TIMEOUT = 120
MAX_DIAGNOSTIC = 262144


class OutputLimitExceeded(ValueError):
    pass


def bounded_diagnostic(command, log_path, *, timeout, cwd=None, env=None, files=()):
    """Bound cooperative POSIX diagnostics, not a hostile-process sandbox.

    Descendants must retain the session/group and inherited resource limits (no
    setsid/setpgid or deliberate rlimit bypass). Kill the entire group on every
    exit, including a normal driver exit, and reap the direct child before return.
    Orphan zombies are the host init's reaping responsibility, never live writers.
    """
    import resource
    import selectors
    import signal
    import time
    def limits():
        resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_DIAGNOSTIC, MAX_DIAGNOSTIC))
    process = None
    count = 0
    deadline = time.monotonic() + timeout
    with log_path.open('xb') as log, selectors.DefaultSelector() as selector:
        os.chmod(log_path, 0o600)
        try:
            process = subprocess.Popen(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, start_new_session=True,
                                       preexec_fn=limits)
            selector.register(process.stdout, selectors.EVENT_READ)
            while True:
                if time.monotonic() >= deadline:
                    raise subprocess.TimeoutExpired(command, timeout)
                # A driver can exit while descendants retain stdout/files.
                if process.poll() is not None:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                ready = selector.select(min(0.02, max(0, deadline-time.monotonic())))
                for key, _ in ready:
                    data = os.read(key.fd, min(65536, MAX_DIAGNOSTIC-count+1))
                    if not data:
                        selector.unregister(key.fileobj)
                    else:
                        log.write(data[:MAX_DIAGNOSTIC-count])
                        count += len(data)
                        if count > MAX_DIAGNOSTIC:
                            raise OutputLimitExceeded()
                for path in files:
                    if path.exists() and path.lstat().st_size >= MAX_DIAGNOSTIC:
                        raise OutputLimitExceeded()
                if process.poll() is not None and not selector.get_map():
                    break
        finally:
            if process is not None:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()
                process.stdout.close()
                # Linux CI: wait for signal delivery to every group member before
                # evidence can be read/sealed. Zombies cannot execute or write;
                # their owning parent/init reaps them. No containment claim for
                # descendants escaping the group or uninterruptible kernel I/O.
                cleanup_deadline = time.monotonic() + 2
                while True:
                    live = False
                    for entry in Path('/proc').iterdir():
                        if not entry.name.isdecimal():
                            continue
                        try:
                            fields = (entry/'stat').read_text().rsplit(')', 1)[1].split()
                            if int(fields[2]) == process.pid and fields[0] not in ('Z', 'X'):
                                live = True
                                break
                        except (OSError, ValueError, IndexError):
                            continue
                    if not live:
                        break
                    if time.monotonic() >= cleanup_deadline:
                        raise OSError('diagnostic-group-not-quiescent')
                    time.sleep(0.005)
    # This runs only after cleanup; size-limit failures are never probe success.
    if process.returncode == -signal.SIGXFSZ or any(p.exists() and p.lstat().st_size >= MAX_DIAGNOSTIC for p in files):
        raise OutputLimitExceeded()
    return process.returncode

PREIMAGE = '3b806c21bd7f0682396c9bda7483cd1f257120a2683ce33e2076b0b3705abc2c'
ANCHOR = '        try:\n            # clean env build folder to avoid issues with following Arduino build\n'
INJECTION = '''        # Crosscub private evidence: preserve generated IDF bytes before deletion.
        if os.environ.get("CROSSCUB_PRIVATE_EVIDENCE"):
            import runpy
            runpy.run_path(str(Path(os.environ["CROSSCUB_PROJECT_SCRIPTS"]) / "ci_producer_capture.py"))["capture_idf"](env)
'''
OLD_CHILD = 'f\'"{pio_exe_path}" run -e "{pio_cmd}" {child_target_args}\''
NEW_CHILD = 'f\'"{pio_exe_path}" run --verbose -e "{pio_cmd}" {child_target_args}\''


def overlay(data):
    text = data.decode()
    if INJECTION in text:
        restored = text.replace(INJECTION, '', 1).replace(NEW_CHILD, OLD_CHILD, 1).encode()
        if hashlib.sha256(restored).hexdigest() != PREIMAGE:
            raise ValueError('unknown-installed-producer')
        return data
    if hashlib.sha256(data).hexdigest() != PREIMAGE or text.count(ANCHOR) != 1 or text.count(OLD_CHILD) != 1:
        raise ValueError('unknown-producer')
    return text.replace(ANCHOR, INJECTION + ANCHOR, 1).replace(OLD_CHILD, NEW_CHILD, 1).encode()


def output_root():
    path = Path(os.environ['CROSSCUB_PRIVATE_EVIDENCE'])
    if not path.is_absolute() or path.is_symlink():
        raise ValueError('private-evidence-path')
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from capture_sticky_startup_config import open_directory
    handle = open_directory(path)
    try:
        os.fchmod(handle, 0o700)
    finally:
        os.close(handle)
    return path


def retain(out, name, path, receipt):
    # Reuse the collector's bounded no-symlink reader; no raw stdout.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from capture_sticky_startup_config import read_regular
    entry = {'source': str(path), 'present': False}
    receipt[name] = entry
    try:
        data = read_regular(path)
        target = out / name
        with target.open('xb') as stream:
            os.chmod(target, 0o600)
            stream.write(data)
        entry.update(present=True, size=len(data), sha256=hashlib.sha256(data).hexdigest())
    except (OSError, ValueError) as exc:
        entry['error'] = type(exc).__name__


def save_receipt(out, name, receipt):
    with (out/name).open('x') as stream:
        os.chmod(out/name, 0o600)
        json.dump(receipt, stream, indent=2)
        stream.write('\n')


def capture_idf(env):
    out = output_root()
    project = Path(env.subst('$PROJECT_DIR'))
    build = Path(env['PROJECT_BUILD_DIR']) / env['PIOENV']
    receipt = {'environment': env['PIOENV'], 'boundary': 'idf_lib_copy-before-rmtree', 'files': {}, 'publication_authorized': False}
    for name, path in {
        'producer-idf-sdkconfig.h': build/'config/sdkconfig.h',
        'producer-idf-sdkconfig.json': build/'config/sdkconfig.json',
        'producer-idf-resolved.sdkconfig': project/('sdkconfig.'+env['PIOENV']),
        'producer-idf-memory.ld': build/'memory.ld',
        'producer-idf-sections.ld': build/'sections.ld',
    }.items():
        retain(out, name, path, receipt['files'])
    save_receipt(out, 'producer-idf-receipt.json', receipt)


def probe_command(args, dependency, macros, target):
    # Retain all actual defines/include/language/ABI flags. Replace only output
    # and dependency-generation switches; preprocessing does not compile/link.
    result = []
    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            i += 2
            continue
        if arg in ('-c', '-MMD', '-MD', '-MP'):
            i += 1
            continue
        if arg.startswith(('-MF', '-MT', '-MQ')):
            i += 1
            continue
        result.append(arg)
        i += 1
    return result + ['-E', '-dM', '-MD', '-MF', str(dependency), '-MT', str(target), '-o', str(macros)]


class CompileAdmissionError(ValueError):
    STAGES = frozenset(('execution-version', 'execution-platform', 'execution-spawn',
                        'execution-shell', 'shell-lexical', 'compile-action'))

    def __init__(self, stage):
        if stage not in self.STAGES:
            raise ValueError('unknown-admission-stage')
        self.stage = stage
        super().__init__('unsupported-compile-admission')


def literal_posix_argv(text):
    """Decode only literal POSIX words. No expansion, evaluation or shell calls.

    Backslash in double quotes removes itself only before $, `, ", or backslash;
    unlike shlex, preserve it before other characters. Deny controls even inside
    quotes, and deny unquoted shell operators/expansion/globbing fail closed.
    """
    if any(ord(c) < 32 or ord(c) == 127 for c in text):
        raise CompileAdmissionError('shell-lexical')
    words, word = [], []
    quote = None
    started = False
    i = 0
    while i < len(text):
        c = text[i]
        if c == '\\' and quote != "'":
            i += 1
            if i == len(text):
                raise CompileAdmissionError('shell-lexical')
            c = text[i]
            if quote == '"' and c not in '$`"\\':
                word.append('\\')
            word.append(c)
            started = True
        elif quote:
            if c == quote:
                quote = None
            elif quote == '"' and c in '$`':
                raise CompileAdmissionError('shell-lexical')
            else:
                word.append(c)
        elif c in "'\"":
            quote = c
            started = True
        elif c == ' ':
            if started:
                words.append(''.join(word))
                word, started = [], False
        elif c in '$`;&|<>()*?[]~#':
            raise CompileAdmissionError('shell-lexical')
        else:
            word.append(c)
            started = True
        i += 1
    if quote:
        raise CompileAdmissionError('shell-lexical')
    if started:
        words.append(''.join(word))
    if not words or not words[0]:
        raise CompileAdmissionError('shell-lexical')
    return words


def effective_compile(env, target, source):
    """Observe pinned SCons execution without spawning or mutating the build env.

    POSIX quote removal only: reject expansion/control syntax, never evaluate a
    shell command. Unknown/multiple actions fail closed for diagnostics only.
    """
    import SCons
    from SCons.Action import Action
    from SCons.Platform.posix import subprocess_spawn
    if SCons.__version__ != '4.8.1':
        raise CompileAdmissionError('execution-version')
    if os.name != 'posix':
        raise CompileAdmissionError('execution-platform')
    if env.get('SPAWN') is not subprocess_spawn:
        raise CompileAdmissionError('execution-spawn')
    if Path(env.get('SHELL', '')).name != 'sh':
        raise CompileAdmissionError('execution-shell')
    # SCons normalizes CR/tab whitespace and splits newline commands before
    # SPAWN. Reject controls before that information can be lost as well.
    if any(ord(c) < 32 or ord(c) == 127
           for c in env.subst('$CXXCOM', target=target, source=source)):
        raise CompileAdmissionError('shell-lexical')
    observed = []
    def observe(shell, escape, cmd, args, execution_env):
        observed.append((literal_posix_argv(' '.join(args)), dict(execution_env)))
        return 0
    # Object-local LTO uses OverrideEnvironment; Clone() on that proxy fails
    # under pinned SCons/Python 3.14. Overlay only SPAWN, keeping its real flags.
    observed_env = env.Override({'SPAWN': observe})
    if Action('$CXXCOM').execute(target, source, observed_env) != 0 or len(observed) != 1:
        raise CompileAdmissionError('compile-action')
    return observed[0]


def install(env):
    if not os.environ.get('CROSSCUB_PRIVATE_EVIDENCE'):
        return
    # Core 6.1.19 reads post scripts after BUILD_SCRIPT constructs objects.
    # A pre-build middleware post-action lives on a temporary Executor which
    # Object() replaces (including the later project-local LTO middleware).
    extra_scripts = env.GetExtraScripts
    script = str(Path(__file__).resolve())
    def with_producer_post_script(build_env, scope):
        paths = extra_scripts(scope)
        return paths + [script] if scope == 'post' else paths
    env.AddMethod(with_producer_post_script, 'GetExtraScripts')


def install_main(env):
    if not os.environ.get('CROSSCUB_PRIVATE_EVIDENCE'):
        return
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    project = Path(env.subst('$PROJECT_DIR')).resolve()
    node = env.File('$BUILD_DIR/src/main.cpp.o')
    # The outer HybridCompile graph contains only .dummy sources. Never create
    # a phantom application executor there or borrow the outer environment.
    if not node.has_builder() or len(node.sources) != 1:
        return
    if Path(node.sources[0].srcnode().get_abspath()).resolve() != project/'src/main.cpp':
        return
    def captured(source, target, env):
        out = output_root()
        receipt = {'environment': env['PIOENV'], 'boundary': 'inner-application-main',
                   'files': {}, 'publication_authorized': False}
        try:
            args, execution_env = effective_compile(env, target, source)
        except Exception as exc:
            # Diagnostic admission must not alter the original compilation.
            receipt['probe_error'] = 'ValueError' if isinstance(exc, CompileAdmissionError) else type(exc).__name__
            if isinstance(exc, CompileAdmissionError) and exc.stage in CompileAdmissionError.STAGES:
                receipt['admission_stage'] = exc.stage
            save_receipt(out, 'producer-main-receipt.json', receipt)
            return 0
        receipt['compile_argv'] = args
        # Exact environment is used internally only. No credential values or
        # environment dump, even in encrypted diagnostics.
        receipt['compile_env_sha256'] = hashlib.sha256(json.dumps(execution_env, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        dep = out/'producer-main.d'
        macros = out/'producer-main.macros'
        command = probe_command(receipt['compile_argv'], dep, macros, str(target[0]))
        receipt['probe_argv'] = command
        try:
            receipt['probe_exit'] = bounded_diagnostic(command, out/'producer-main-preprocess.log',
                cwd=project, env=execution_env, timeout=PROBE_TIMEOUT, files=(dep, macros))
        except Exception as exc:
            receipt['probe_error'] = type(exc).__name__
        # Freeze selected header bytes at the actual compiler boundary.
        if receipt.get('probe_exit') == 0:
            from capture_sticky_startup_config import parse_dependencies, read_regular
            try:
                paths = parse_dependencies(read_regular(dep).decode(), str(target[0]))
                headers = {str((project/p).resolve()) for p in paths if Path(p).name == 'sdkconfig.h'}
                if len(headers) != 1:
                    raise ValueError('header-not-unique')
                retain(out, 'producer-application-sdkconfig.h', Path(headers.pop()), receipt['files'])
            except (OSError, ValueError, UnicodeError) as exc:
                receipt['selection_error'] = type(exc).__name__
        from capture_sticky_startup_config import read_regular
        for path in (dep, macros):
            try:
                data = read_regular(path)
                os.chmod(path, 0o600)
                if 'probe_error' not in receipt:
                    receipt[path.name+'_sha256'] = hashlib.sha256(data).hexdigest()
            except (OSError, ValueError) as exc:
                receipt[path.name+'_error'] = type(exc).__name__
        retain(out, 'producer-application-main.d', Path(str(target[0])).with_suffix('.d'), receipt['files'])
        save_receipt(out, 'producer-main-receipt.json', receipt)
        # Diagnostic failure is admitted only by the existing fail-closed collector.
        return 0
    # This is the final Object executor, with its object-local flags/ENV.
    env.AddPostAction(node, captured)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--producer', type=Path, required=True)
    args = parser.parse_args()
    before = args.producer.read_bytes()
    after = overlay(before)
    out = output_root()
    pristine = after.decode().replace(INJECTION, '', 1).replace(NEW_CHILD, OLD_CHILD, 1)
    diff = ''.join(difflib.unified_diff(pristine.splitlines(True), after.decode().splitlines(True), fromfile='platform/builder/frameworks/espidf.py', tofile='platform/builder/frameworks/espidf.py'))
    with (out/'producer-overlay.patch').open('x') as stream:
        os.chmod(out/'producer-overlay.patch', 0o600)
        stream.write(diff)
    args.producer.write_bytes(after)


if 'Import' in globals():
    Import('env')
    # SCons does not define __file__; runpy supplies it to the actual helper.
    import runpy
    runpy.run_path(str(Path(env.subst('$PROJECT_DIR'))/'scripts/ci_producer_capture.py'))['install_main'](env)
elif __name__ == '__main__':
    main()
