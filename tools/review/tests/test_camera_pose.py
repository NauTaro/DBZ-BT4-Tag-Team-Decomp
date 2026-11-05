"""Projection-independent camera motion at fractional presentation times."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class CameraPoseTests(unittest.TestCase):
 def test_pose_roundtrip_motion_zoom_wrap_stationary_and_invalid_projection(self):
  cc=shutil.which('gcc') or shutil.which('cc')
  if not cc:self.skipTest('C compiler required')
  code='#include <assert.h>\n#include <stdio.h>\n#include "'+(ROOT/'port/src/gs/gs_camera_interp.h').as_posix()+'"\n'+r'''
/* Independent yaw/pitch view and native GS-like projection. */
static void make(double yaw,double pitch,const double *eye,double fx,double fy,double *m){
 double cy=cos(yaw),sy=sin(yaw),cp=cos(pitch),sp=sin(pitch);
 double right[3]={cy,0,-sy},up[3]={sy*sp,cp,cy*sp},forward[3]={sy*cp,-sp,cy*cp};
 for(int j=0;j<3;++j){m[j*4]=fx*right[j]+2048*forward[j];m[j*4+1]=fy*up[j]+2048*forward[j];m[j*4+2]=-16384*forward[j];m[j*4+3]=forward[j];}
 for(int j=0;j<4;++j)m[12+j]=-m[j]*eye[0]-m[4+j]*eye[1]-m[8+j]*eye[2];
 m[14]+=16777216;
}
static void equal(const double *a,const double *b){
 for(int i=0;i<16;++i){if(fabs(a[i]-b[i])>1e-8*(1+fabs(b[i]))){fprintf(stderr,"matrix %d %.12g != %.12g\n",i,a[i],b[i]);assert(0);}}
}
int main(void){
 double a[16],b[16],o[16],expected[16],eye[3],next[3],half[3];GsCameraPose pose;
 for(int k=0;k<1000;++k){
  double yaw=-3.14+(k%201)*.0313,pitch=-1.3+(k%83)*.031;
  for(int j=0;j<3;++j){eye[j]=(k-500)*(j+1)*137.3;next[j]=eye[j]+(j-1)*.7;half[j]=(eye[j]+next[j])*.5;}
  for(int sign=-1;sign<=1;sign+=2){
   make(yaw,pitch,eye,1900,sign*1600,a);assert(GsCamera_Extract(a,&pose));GsCamera_Matrix(&pose,o);equal(o,a);
   make(yaw+.008,pitch,next,1920,sign*1640,b);
   assert(GsCamera_Blend(a,b,0,o));assert(!memcmp(o,a,sizeof(a)));
   assert(GsCamera_Blend(a,b,1,o));assert(!memcmp(o,b,sizeof(b)));
   assert(GsCamera_Blend(a,a,.5,o));assert(!memcmp(o,a,sizeof(a)));
   assert(GsCamera_Blend(a,b,.5,o));make(yaw+.004,pitch,half,1910,sign*1620,expected);equal(o,expected);
  }
 }
 eye[0]=eye[1]=eye[2]=0;
 make(3.13,.1,eye,1700,-1700,a);make(-3.13,.1,eye,1700,-1700,b);
 assert(GsCamera_Blend(a,b,.5,o));make(acos(-1),.1,eye,1700,-1700,expected);equal(o,expected);
 a[0]=NAN;assert(!GsCamera_Blend(a,b,.5,o));
 memset(a,0,sizeof(a));a[0]=a[5]=a[10]=a[15]=1;assert(!GsCamera_Blend(a,a,.5,o));
 assert(!GsCamera_Blend(b,b,-.1,o));
 puts("Camera pose: 2000 native-like GS projections, motion/zoom, exact stationary/endpoints, shortest turn, invalid fallback PASS");
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   d=Path(tmp);src=d/'probe.c';exe=d/('probe.exe' if os.name=='nt' else 'probe');src.write_text(code)
   kwargs={'creationflags':0x08000000} if os.name=='nt' else {}
   p=subprocess.run([cc,'-O2',str(src),'-lm','-o',str(exe)],capture_output=True,text=True,**kwargs)
   self.assertEqual(p.returncode,0,p.stdout+p.stderr)
   p=subprocess.run([str(exe)],capture_output=True,text=True,**kwargs);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
