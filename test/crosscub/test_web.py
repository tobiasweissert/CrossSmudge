#!/usr/bin/env python3
from pathlib import Path
import ast, re, subprocess, tempfile, unittest
R=Path(__file__).resolve().parents[2]

class WebBrand(unittest.TestCase):
    def test_generated_portal_identity(self):
        code=(R/'scripts/build_web.py').read_text().split('# Cache-busting version')[0]
        scope={'__file__':str(R/'scripts/build_web.py')}
        exec(compile(code,'build_web.py','exec'),scope)
        for slug,(_,title,active,extra) in scope['PAGES'].items():
            values={'title':title,'v':'test','head_extra':extra,'styles':'','body':(R/'web/pages'/f'{slug}.html').read_text(),'script':''}
            values.update({f'cls_{p}':'' for p in scope['PAGES']})
            html=scope['render']((R/'web/templates/base.html').read_text(),values)
            self.assertIn('Crosscub',title)
            self.assertIn('Crosscub &bull; Open Source',html)
            self.assertNotIn('CrossSmudge',html)

    def test_converter_uses_only_sticky_dimensions(self):
        s=(R/'web/pages/files.js').read_text()
        declarations=s[s.index('const DEVICE_PROFILES'):s.index('const UPLOAD_SETTINGS_STORAGE_KEY')]
        function=re.search(r'function applyDeviceTarget\(\) \{.*?\n\}',s,re.S).group()
        with tempfile.TemporaryDirectory(prefix='crosscub-web-') as tmp:
            js=Path(tmp)/'web.js'
            js.write_text('const document={getElementById:()=>null,querySelectorAll:()=>[]};let epubImagesCache=[];\n'+declarations+'\n'+function+'\napplyDeviceTarget();if(ACTIVE_DEVICE!=="Sticky"||MAX_WIDTH!==480||MAX_HEIGHT!==800)throw Error("converter did not target Sticky");if(Object.keys(DEVICE_PROFILES).join()!=="Sticky")throw Error("other device selectable");')
            subprocess.run(['node',str(js)],check=True)

if __name__=='__main__':unittest.main()
