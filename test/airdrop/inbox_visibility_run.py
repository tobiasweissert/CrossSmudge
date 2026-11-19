#!/usr/bin/env python3
"""Exercise production Inbox/browser filters and extension helpers on the host."""
from pathlib import Path
import re, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
source = (root / 'src/activities/home/FileBrowserActivity.cpp').read_text()
fs = (root / 'lib/FsHelpers/FsHelpers.cpp').read_text()
def function(text, name):
    match = re.search(r'bool ' + name + r'\([^)]*\) \{', text)
    assert match, name
    end = text.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]
helpers = '\n'.join(function(fs, n) for n in ['checkFileExtension', 'hasEpubExtension', 'hasXtcExtension', 'hasTxtExtension', 'hasMarkdownExtension', 'hasBmpExtension', 'hasPngExtension'])
filters = '\n'.join(function(source, n) for n in ['equalsIgnoreCase', 'isMacOSMetadataEntry', 'isWindowsMetadataEntry', 'isSupportedBrowserFile', 'acceptInbox', 'acceptCommon', 'isInboxPath', 'acceptFirmware', 'acceptDirectory'])
# Both in-memory listings and the large-directory index must choose the Inbox filter.
assert source.count('isInboxPath(basepath) ? acceptInbox : acceptCommon') == 2
program = r"""
#include <string_view>
#include <cstring>
#include <cctype>
#include <cassert>
#include <cstdio>
struct { bool showHiddenFiles = false; } SETTINGS;
namespace FsHelpers { HELPERS }
FILTERS
int main() {
    for (auto path : {"/Inbox", "/inBOX/", "/Inbox///"}) assert(isInboxPath(path));
    for (auto path : {"/", "/Books", "/Inbox2", "/Books/Inbox", "/Inbox/.airdrop"}) assert(!isInboxPath(path));
    for (auto name : {"received.pdf", "PHOTO.JPG", "font.otf", "archive.zip", "binary.bin", "script.sh", "LICENSE"}) {
        assert(acceptInbox(name, false));
        assert(!acceptCommon(name, false));
        assert(!acceptDirectory(name, false));
    }
    for (auto name : {"book.epub", "book.xtc", "book.txt", "notes.md", "image.png"}) assert(acceptCommon(name, false));
    assert(acceptCommon("folder", true));
    assert(!acceptInbox(".airdrop", true));
    assert(!acceptInbox("._received.pdf", false));
    SETTINGS.showHiddenFiles = true;
    assert(acceptInbox(".airdrop", true));
    assert(!acceptInbox(".DS_Store", false));
    assert(!acceptInbox("._received.pdf", false));
    assert(!acceptInbox("Thumbs.db", false));
    assert(acceptFirmware("update.bin", false));
    assert(!acceptFirmware("received.pdf", false));
    puts("PASS production Inbox visibility, reader filters, hidden/metadata and picker boundaries");
}
""".replace('HELPERS', helpers).replace('FILTERS', filters)
with tempfile.TemporaryDirectory(prefix='airdrop-inbox-') as td:
    path = Path(td)
    (path / 'test.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
