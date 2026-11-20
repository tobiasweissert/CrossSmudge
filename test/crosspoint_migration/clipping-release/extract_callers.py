"""Compile exact production Rename method and confirmed-Delete callback body.
UI scheduling/rendering dependencies are observation stubs, not storage models.
"""
import pathlib, sys
root=pathlib.Path(__file__).resolve().parents[3]
source=(root/'src/activities/home/FileBrowserActivity.cpp').read_text()
def between(start,end):
    assert source.count(start)==1 and source.count(end)==1
    return source.split(start,1)[1].split(end,1)[0]
rename='void FileBrowserActivity::renameFile'+between('void FileBrowserActivity::renameFile','void FileBrowserActivity::toggleHiddenFiles')
source=between('void FileBrowserActivity::promptDeleteFile(', 'void FileBrowserActivity::promptDeleteDirectory(')
delete=between('  auto handler = [this, fullPath](const ActivityResult& res) {\n','  const std::string heading = tr(STR_DELETE)')
assert delete.rstrip().endswith('};')
delete=delete.rstrip()[:-2]
parts=[(pathlib.Path(__file__).parent/'caller_prefix.h').read_text(),rename,
 'void FileBrowserActivity::deleteConfirmed(const std::string& fullPath, const ActivityResult& res) {\n'+delete+'}\n',
 (pathlib.Path(__file__).parent/'caller_tests.inc').read_text()]
pathlib.Path(sys.argv[1]).write_text('\n'.join(parts))
