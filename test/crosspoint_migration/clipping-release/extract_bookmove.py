import pathlib, sys
T=pathlib.Path(__file__).resolve().parent
R=T.parents[2]
s=(R/'src/util/BookMoveUtils.cpp').read_text()
helpers=s.split('namespace {',1)[1].split('}  // namespace',1)[0]
start='RenameMigrationResult migrateRenamedBookState('
assert s.count(start)==1
method=start+s.split(start,1)[1].split('bool migrateMovedEpubState(',1)[0]
pathlib.Path(sys.argv[1]).write_text((T/'bookmove_prefix.h').read_text()+'\nnamespace {\n'+helpers+'}\nnamespace BookMoveUtils {\n'+method+'}\n'+(T/'bookmove_tests.inc').read_text())
