#!/usr/bin/env python3
"""Prepare an owner-approved release; never commit, push, build or dispatch locally."""
import argparse
import configparser
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

from validate_ota_release import REPOSITORY, validate_release, validate_version


def set_version(path, version):
    """Portable targeted edit; preserve comments and all other INI bytes."""
    validate_version(version)
    original = path.read_bytes()
    text = original.decode('utf-8')
    for section in ['crosssmudge', 'crossink']:
        pattern = rf'(^\[{section}\][^\r\n]*\r?\n)(.*?)(?=^\[|\Z)'
        matches = list(re.finditer(pattern, text, re.M | re.S))
        if len(matches) != 1:
            raise ValueError('expected exactly one [' + section + '] section')
        match = matches[0]
        body, count = re.subn(r'(?m)^(version\s*=\s*)[^\r\n]*', lambda m: m[1] + version, match[2])
        if count != 1:
            raise ValueError('expected exactly one version in [' + section + ']')
        text = text[:match.start(2)] + body + text[match.end(2):]
    path.write_bytes(text.encode('utf-8'))


def qualify_source(version, *, repository, event, ref, actor, publish, source_sha, cwd=Path('.')):
    validate_version(version)
    if repository != REPOSITORY:
        raise ValueError('release repository must be ' + REPOSITORY)
    if event not in ['workflow_dispatch', 'push']:
        raise ValueError('only explicit release dispatch or release tag push is supported')
    if event == 'workflow_dispatch' and ref != 'main':
        raise ValueError('manual releases must select main')
    if event == 'push' and ref != 'crosscub-v' + version:
        raise ValueError('tag/version mismatch')
    if publish and (event != 'workflow_dispatch' or actor != 'tobiasweissert'):
        raise ValueError('stable publication requires explicit repository-owner dispatch')
    def git(*args):
        result = subprocess.run(['git', *args], cwd=cwd, capture_output=True, text=True)
        if result.returncode:
            raise ValueError('release source/target check failed: git ' + ' '.join(args))
        return result.stdout.strip()
    if git('rev-parse', 'HEAD') != source_sha:
        raise ValueError('checkout must match the workflow source SHA exactly')
    git('merge-base', '--is-ancestor', source_sha, 'refs/remotes/origin/main')
    tag = 'refs/tags/crosscub-v' + version
    exists = subprocess.run(['git', 'show-ref', '--verify', '--quiet', tag], cwd=cwd).returncode
    if exists == 0 and git('rev-parse', tag + '^{commit}') != source_sha:
        raise ValueError('existing tag targets different source')
    if exists not in [0, 1]:
        raise ValueError('cannot inspect existing tag')
    config = configparser.ConfigParser(interpolation=None)
    config.read(Path(cwd) / 'platformio.ini')
    for section in ['crosssmudge', 'crossink']:
        if config.get(section, 'version', fallback=None) != version:
            raise ValueError('commit matching version fields before release: [' + section + ']')


def github(endpoint, *, fields=None):
    """Only exact HTTP 404 means absent; auth/rate-limit/network failures stop release."""
    command = ['gh', 'api', '--hostname', 'github.com', 'repos/' + REPOSITORY + '/' + endpoint]
    if fields is not None:
        command.extend(['--method', 'POST'])
        for key, value in fields.items():
            command.extend(['--raw-field', key + '=' + value])
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        if '(HTTP 404)' in result.stderr:
            return None
        raise ValueError('GitHub release lookup failed (not a confirmed 404)')
    return json.loads(result.stdout)


def check_remote(version, *, skip_published=False):
    validate_version(version)
    existing = github('releases/tags/crosscub-v' + version)
    if existing is not None:
        if (skip_published and existing.get('tag_name') == 'crosscub-v' + version
                and existing.get('draft') is False and existing.get('prerelease') is False):
            print('Already published stable release: no build, upload, redraft or replacement.')
            return False
        raise ValueError('release already exists; never overwrite or redraft it (including duplicate tag workflows)')
    latest = github('releases/latest')
    if latest is not None:
        tag = latest.get('tag_name', '')
        if not tag.startswith('crosscub-v'):
            raise ValueError('latest release does not follow Crosscub stable identity')
        old = validate_version(tag[len('crosscub-v'):])
        def numeric(value):
            parts = [int(part) for part in value.split('.')]
            return tuple(parts + [0] * (4 - len(parts)))
        if numeric(version) <= numeric(old):
            raise ValueError('release must be numerically newer than GitHub /latest')


def ensure_remote_tag(tag, source_sha):
    ref = github('git/ref/tags/' + tag)
    if ref is None:
        github('git/refs', fields={'ref': 'refs/tags/' + tag, 'sha': source_sha})
        # A draft release does not reliably create its tag until publication.
        # Create the immutable target explicitly, then verify the exact ref.
        ref = github('git/ref/tags/' + tag)
    obj = ref.get('object', {}) if ref else {}
    for _ in range(4):
        if obj.get('type') != 'tag':
            break
        annotation = github('git/tags/' + obj['sha'])
        obj = annotation.get('object', {}) if annotation else {}
    if obj.get('type') != 'commit' or obj.get('sha') != source_sha:
        raise ValueError('GitHub tag does not resolve to qualified source SHA')


def release_command(args):
    subprocess.run(['gh', 'release', *args], check=True, env={**os.environ, 'GH_HOST': 'github.com'})


def upload_release(version, firmware, source_sha, *, publish=False):
    """CI only: validate, create a NEW draft, verify target/asset, then opt-in publish."""
    report = validate_release(firmware, version)
    check_remote(version)
    tag = report['tag']
    ensure_remote_tag(tag, source_sha)
    release_command(['create', tag, str(firmware), '--repo', REPOSITORY, '--target', source_sha,
                     '--verify-tag', '--draft', '--prerelease=false', '--latest=false', '--title', 'Crosscub Firmware v' + version,
                     '--notes', 'Owner-qualified Crosscub Sticky OTA update. SHA256: ' + report['sha256']])
    def verify(draft):
        release = github('releases/tags/' + tag)
        if not release or release.get('tag_name') != tag or release.get('draft') is not draft or release.get('prerelease') is not False:
            raise ValueError('GitHub release state read-back mismatch')
        assets = release.get('assets', [])
        expected_url = f'https://github.com/{REPOSITORY}/releases/download/{tag}/{report["filename"]}'
        if len(assets) != 1 or any([
                assets[0].get('name') != report['filename'], assets[0].get('size') != report['size'],
                assets[0].get('digest') != 'sha256:' + report['sha256'],
                assets[0].get('browser_download_url') != expected_url]):
            raise ValueError('GitHub uploaded asset name/size/digest/owner URL mismatch; do not publish')
        ref = github('git/ref/tags/' + tag)
        obj = ref.get('object', {}) if ref else {}
        for _ in range(4):
            if obj.get('type') != 'tag':
                break
            annotation = github('git/tags/' + obj['sha'])
            obj = annotation.get('object', {}) if annotation else {}
        if obj.get('type') != 'commit' or obj.get('sha') != source_sha:
            raise ValueError('GitHub tag does not resolve to qualified source SHA')
        return release
    verify(True)
    if publish:
        release_command(['edit', tag, '--repo', REPOSITORY, '--draft=false', '--prerelease=false', '--latest'])
        release = verify(False)
        latest = github('releases/latest')
        if not latest or latest.get('tag_name') != tag or latest.get('draft') is not False or latest.get('prerelease') is not False:
            raise ValueError('published release was not confirmed at GitHub /latest')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version', nargs='?')
    parser.add_argument('--ci-check', action='store_true', help='CI: check owner/source/version/remote release before building')
    parser.add_argument('--ci-upload', type=Path, help='CI: qualify and upload this artifact; draft unless explicitly approved')
    parser.add_argument('--publish', action='store_true', help='Print owner command selecting stable /latest publication; default is draft')
    parser.add_argument('--write-version', action='store_true', help='Explicitly update both tracked version fields; does not commit/tag/push')
    parser.add_argument('--config', type=Path, default=Path(__file__).resolve().parents[1] / 'platformio.ini')
    args = parser.parse_args()
    try:
        if args.ci_check or args.ci_upload:
            if os.environ.get('GITHUB_ACTIONS') != 'true' or args.write_version:
                raise ValueError('CI mode requires GitHub Actions and forbids changing source versions')
            event = os.environ.get('GITHUB_EVENT_NAME', '')
            ref = os.environ.get('GITHUB_REF_NAME', '')
            version = args.version or (os.environ.get('INPUT_VERSION', '') if event == 'workflow_dispatch' else ref.removeprefix('crosscub-v'))
            publish_value = os.environ.get('PUBLISH_RELEASE', 'false')
            if publish_value not in ['true', 'false', '']:
                raise ValueError('PUBLISH_RELEASE must be true or false')
            publish = args.publish or publish_value == 'true'
            qualify_source(version, repository=os.environ.get('GITHUB_REPOSITORY', ''), event=event,
                           ref=ref, actor=os.environ.get('GITHUB_ACTOR', ''), publish=publish,
                           source_sha=os.environ.get('GITHUB_SHA', ''))
            if args.ci_upload:
                report = upload_release(version, args.ci_upload, os.environ['GITHUB_SHA'], publish=publish)
                print(json.dumps(report, indent=2))
            else:
                proceed = check_remote(version, skip_published=True)
                with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
                    output.write('release_version=' + version + '\n')
                    output.write('skip_release=' + ('true' if proceed is False else 'false') + '\n')
            return
        validate_version(args.version)
        if args.write_version:
            set_version(args.config, args.version)
    except (ValueError, OSError, configparser.Error, subprocess.CalledProcessError) as exc:
        parser.exit(1, str(exc) + '\n')
    print('Review and commit the version and qualified source, then push approved main to ' + REPOSITORY + '.')
    print('No build, Git operation, workflow dispatch or publication was performed.')
    print('After approval, run explicitly:')
    print(shlex.join(['gh', 'workflow', 'run', 'release.yml', '--repo', REPOSITORY, '--ref', 'main',
                      '-f', 'version=' + args.version, '-f', 'publish=' + str(args.publish).lower()]))


if __name__ == '__main__':
    main()
