"""Only submitted inputs can teach the client that its network lead was too small."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

def feedback_function():
    source=(ROOT/'port/src/gs/tag_team_sync_client.inc').read_text(encoding='utf8')
    start=source.index('static void tag_delay_feedback(')
    brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

class TagInputFeedbackTests(unittest.TestCase):
    def test_unsampled_frames_do_not_inflate_network_lead(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        code='''#include <assert.h>
static struct {unsigned epoch;} state={4};
static int member=101,seat=0,tag_paced=1,tag_input_sent=1;
static int tag_delay_margin=1,tag_delay_adjusted;
static unsigned tag_input_epoch=4,tag_input_first=100,tag_input_latest=110;
static unsigned tag_delay_last_adjust_frame,tag_delay_clean_frames;
static int Tt_Seat(const void *room,int id){return seat;}
'''+feedback_function()+'''
int main(void){
    unsigned frame;
    /* Frames committed while this client never sampled or sent their input.
       A backlog of such notices cannot turn a local stall into network delay. */
    for(frame=111;frame<211;++frame)tag_delay_feedback(frame,1);
    assert(tag_delay_margin==1&&!tag_delay_adjusted&&tag_delay_clean_frames==0);
    /* Startup padding is not a late packet either. */
    tag_delay_feedback(99,1);assert(tag_delay_margin==1);
    /* Actual submitted input still raises the lead, once per feedback window. */
    tag_delay_feedback(100,1);assert(tag_delay_margin==2&&tag_delay_adjusted);
    for(frame=101;frame<108;++frame)tag_delay_feedback(frame,1);
    assert(tag_delay_margin==2);
    tag_delay_feedback(108,1);assert(tag_delay_margin==3);
    /* The inclusive upper bound is a real submitted frame. */
    tag_delay_adjusted=0;tag_delay_feedback(110,1);assert(tag_delay_margin==4);
    tag_delay_feedback(111,1);assert(tag_delay_margin==4);
    /* A later submission can make that next frame eligible for feedback. */
    tag_input_latest=200;tag_delay_adjusted=0;
    tag_delay_feedback(111,1);assert(tag_delay_margin==5);
    /* Another player's late input cannot increase this seat's lead. */
    tag_delay_adjusted=0;tag_delay_feedback(112,2);assert(tag_delay_margin==5);
    seat=3;tag_delay_feedback(113,1);assert(tag_delay_margin==5);
    tag_delay_feedback(114,8);assert(tag_delay_margin==6);
    /* Old epochs, absent local input and missing membership remain ignored. */
    tag_input_epoch=3;tag_delay_adjusted=0;tag_delay_feedback(115,8);
    assert(tag_delay_margin==6&&!tag_delay_adjusted);
    tag_input_epoch=4;tag_input_sent=0;tag_delay_feedback(115,8);assert(tag_delay_margin==6);
    tag_input_sent=1;seat=-1;tag_delay_feedback(115,8);assert(tag_delay_margin==6);
    /* Existing healthy-frame recovery remains in force. */
    seat=0;tag_delay_clean_frames=0;tag_delay_margin=3;tag_input_latest=500;
    for(frame=100;frame<442;++frame)tag_delay_feedback(frame,0);
    assert(tag_delay_margin==2&&tag_delay_clean_frames==0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);src=root/'feedback.c';exe=root/('feedback.exe' if os.name=='nt' else 'feedback')
            src.write_text(code,encoding='utf8');flags={'creationflags':0x08000000} if os.name=='nt' else {}
            p=subprocess.run([cc,'-std=c99',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=30,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)

if __name__=='__main__':unittest.main()
