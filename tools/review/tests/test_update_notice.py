"""Localized notes and menu-only visibility use production updater helpers."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[3]


class UpdateNoticeTests(unittest.TestCase):
    def test_release_has_all_three_summaries(self):
        notes = (ROOT / 'CHANGELOG.md').read_text(encoding='utf8').split('\n# ', 1)[0]
        for locale in ('es', 'en', 'pt'):
            marker = '<!-- bt4-update:' + locale + '\n'
            self.assertIn(marker, notes)
            self.assertGreater(len(notes.split(marker, 1)[1].split('-->', 1)[0]), 100)

    def test_locale_fallback_bounds_and_every_menu_page(self):
        cc = shutil.which('cc') or shutil.which('gcc')
        if not cc:
            self.skipTest('A C compiler is required')
        code = r'''#include <assert.h>
#include "update_notes.h"
int main(void){
 char out[256],small[4];
 const char *notes="English fallback\n<!-- bt4-update:es\nVida azul\n-->\n"
 "<!-- bt4-update:pt\r\nVida correta\r\n-->\n<!-- bt4-update:en\nBlue health\n-->";
 UpdNotes_Select(notes,"es",out,sizeof(out));assert(!strcmp(out,"Vida azul"));
 UpdNotes_Select(notes,"pt",out,sizeof(out));assert(!strcmp(out,"Vida correta"));
 UpdNotes_Select(notes,"en",out,sizeof(out));assert(!strcmp(out,"Blue health"));
 UpdNotes_Select(notes,"fr",out,sizeof(out));assert(!strcmp(out,"English fallback"));
 UpdNotes_Select(NULL,"es",out,sizeof(out));assert(!*out);
 UpdNotes_Select("No metadata","es",out,sizeof(out));assert(!strcmp(out,"No metadata"));
 UpdNotes_Select("\xE2\x82\xAC\xE2\x82\xAC","en",small,sizeof(small));
 assert(strlen(small)==3);assert(!memcmp(small,"\xE2\x82\xAC",3));
 UpdNotes_Select("<!-- bt4-update:es\nbroken","es",out,sizeof(out));assert(!*out);
 for(int page=0;page<=54;++page){
  assert(!UpdUi_MenuAllowed(0,page)); /* gameplay, pause and cinematics */
  int selector=page==3||page==15||page==19||page==23||page==24;
  int startup=page<2||page==50||page==51;
  assert(UpdUi_MenuAllowed(1,page)==!(selector||startup));
 }
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            src = path / 'notice.c'
            src.write_text(code)
            exe = path / ('notice.exe' if os.name == 'nt' else 'notice')
            prefix = ['rtk', 'proxy'] if shutil.which('rtk') else []
            flags = {'creationflags': 0x08000000} if os.name == 'nt' else {}
            result = subprocess.run(prefix + [cc, '-std=c99', '-I'+str(ROOT/'port/src/gs'),
                str(src), '-o', str(exe)], capture_output=True, text=True, **flags)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run(prefix + [str(exe)], capture_output=True, text=True, **flags)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
