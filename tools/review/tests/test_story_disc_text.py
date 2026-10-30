from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class StoryDiscTextTests(unittest.TestCase):
    def test_original_disc_spelling_and_unchanged_unrelated_text(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc:self.skipTest('C compiler unavailable')
        root=Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)
            def units(text):return ','.join(map(str,[0xfeff,*map(ord,text),0]))
            source='''#include "port/src/gs/ui_text_translate.c"
#include <assert.h>
int main(void){
const uint16_t original[]={ORIGINAL};
const uint16_t expected[]={EXPECTED};
const uint16_t english[]={ENGLISH};
const uint16_t short_text[]={0xfeff,'L',0};
assert(ui_equal(UiText_TranslateString(original),expected));
assert(ui_equal(UiText_TranslateLine(NULL,0,original),expected));
assert(original[8]=='a');
assert(UiText_TranslateString(english)==english);
assert(UiText_TranslateString(short_text)==short_text);
assert(UiText_TranslateString(NULL)==NULL);
return 0;
}'''.replace('ORIGINAL',units('La Barrara a Superar')).replace('EXPECTED',units('La Barrera a Superar')).replace('ENGLISH',units('The Barrier to Surpass'))
            c=p/'story.c';exe=p/'story.exe';c.write_text(source,encoding='utf8')
            subprocess.run([cc,'-std=c99','-I',str(root),str(c),'-o',str(exe)],check=True,capture_output=True)
            subprocess.run([str(exe)],check=True,capture_output=True)
