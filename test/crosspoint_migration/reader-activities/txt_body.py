from activity_bodies import method

def emit(root, out):
    text=(root/'src/activities/reader/TxtReaderActivity.cpp').read_text()
    code='''#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
#include <HalStorage.h>
struct Txt {std::string getCachePath() const {return "/txt";} size_t getFileSize() const {return 1000;}};
class TxtReaderActivity {public:
 Txt object; Txt* txt=&object;
 int currentPage=0, totalPages=5;
 std::vector<size_t> pageOffsets{0,60,120,180,240};
 void loadProgress();
};
'''+method(text,'TxtReaderActivity::loadProgress')+'''
int main() {
 HalFile f; Storage.openFileForWrite("TEST", "/txt/progress.bin", f);
 const uint8_t saved[]={2,0,200,0,0,0}; f.write(saved,sizeof(saved)); f.close();
 TxtReaderActivity reader; reader.loadProgress();
 assert(reader.currentPage==3); // page containing raw offset 200 after font/viewport reflow
 // Legacy four-byte positions still retain their page fallback.
 Storage.openFileForWrite("TEST", "/txt/progress.bin", f);
 const uint8_t old[]={1,0,0,0}; f.write(old,sizeof(old)); f.close();
 reader.loadProgress(); assert(reader.currentPage==1);
 puts("PASS actual TXT progress reader preserves raw offset through font/viewport reflow and legacy fallback");
}
'''
    (out/'activities.cpp').write_text(code)
