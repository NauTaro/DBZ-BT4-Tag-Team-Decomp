"""BT4 bilingual file ranges keep the BT3 fallback and reject unrelated files."""
import unittest
import test_r12_tag_crashes as probes

class VoiceFileTests(unittest.TestCase):
    def test_both_rosters_stream_boundaries_and_voice_banks(self):
        code=r'''
#include "gs_voice_files.h"
#include <assert.h>
int main(void){
 for(int c=0;c<250;++c)for(int line=0;line<100;++line)
  assert(PortVoice_OtherStream(0x16E6A+c*100+line,1)==0x10CC2+c*100+line);
 for(int c=0;c<250;++c){
  assert(PortVoice_Bank(c,0,1)==0x1D10C+c);
  assert(PortVoice_Bank(c,1,1)==0x1D012+c);
 }
 for(int c=0;c<161;++c){
  assert(PortVoice_Bank(c,0,0)==0xC7B+c);
  assert(PortVoice_Bank(c,1,0)==0xBDA+c);
 }
 assert(!PortVoice_OtherStream(-1,1)&&!PortVoice_OtherStream(0x16E69,1)&&!PortVoice_OtherStream(0x1D012,1));
 assert(PortVoice_OtherStream(0xCC32,0)==0x8D4E&&!PortVoice_OtherStream(0x8D30,0));
 assert(PortVoice_OtherStream(0x10B6C,0)==0x10B34&&PortVoice_OtherStream(0x10BA3,0)==0x10B6B);
 assert(!PortVoice_Bank(-1,0,1)&&!PortVoice_Bank(250,0,1)&&!PortVoice_Bank(161,0,0));
}
'''
        code=code.replace('"gs_voice_files.h"','"'+(probes.ROOT/'port/src/gs/gs_voice_files.h').as_posix()+'"')
        probes.ReportedTagCrashes.native(self,code)

if __name__=='__main__':unittest.main()
