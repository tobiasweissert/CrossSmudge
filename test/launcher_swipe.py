"""Compile the production swipe block with deterministic input and layout doubles."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/activities/apps/ApplicationsActivity.h').read_text()
swipe = source[source.index('    const auto swipe ='):source.index('    auto launchSelectedApp =')]
geometry = source[source.index('  int visibleRowsPerPage() const {'):source.index('  void refreshMenuList()')]
program = r'''
#include <algorithm>
#include <cassert>
struct MappedInputManager {
 enum class SwipeDir {None,Up,Down};
 SwipeDir dir=SwipeDir::None;
 SwipeDir wasSwipe()const{return dir;}
};
struct Metrics {int topPadding=16,headerHeight=50,verticalSpacing=8,buttonHintsHeight=40,menuSpacing=8,menuRowHeight=56;};
struct UITheme {static UITheme& getInstance(){static UITheme t;return t;} Metrics getMetrics()const{return {};}};
struct Renderer {int height=800;int getScreenHeight()const{return height;}};
struct App {
 MappedInputManager mappedInput;Renderer renderer;int selectedIndex=0,updates=0;
 void requestUpdate(){++updates;}
 GEOMETRY
 void loop(int appCount){SWIPE}
};
int main(){
 App a;assert(a.visibleRowsPerPage()==10);
 a.mappedInput.dir=MappedInputManager::SwipeDir::Up;
 a.loop(29);assert(a.selectedIndex==10 && a.updates==1);
 a.loop(29);assert(a.selectedIndex==20);
 a.loop(29);assert(a.selectedIndex==28);
 a.loop(29);assert(a.selectedIndex==28 && a.updates==3);
 a.mappedInput.dir=MappedInputManager::SwipeDir::Down;
 a.loop(29);assert(a.selectedIndex==18);
 a.selectedIndex=1;a.loop(29);assert(a.selectedIndex==0);
 a.renderer.height=480;assert(a.visibleRowsPerPage()==5);
 a.mappedInput.dir=MappedInputManager::SwipeDir::Up;a.loop(29);assert(a.selectedIndex==5);
 a.selectedIndex=0;a.loop(1);assert(a.selectedIndex==0);
}
'''.replace('GEOMETRY', geometry).replace('SWIPE', swipe)
with tempfile.TemporaryDirectory() as folder:
    path = Path(folder)
    (path / 'test.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS production launcher swipe block: paging, clamping, landscape, single app')
