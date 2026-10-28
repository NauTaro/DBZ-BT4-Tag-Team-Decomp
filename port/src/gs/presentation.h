#ifndef BT4_PRESENTATION_H
#define BT4_PRESENTATION_H
#ifdef __cplusplus
extern "C" {
#endif
int PortHud_Hidden(void);
void PortHud_Toggle(void);
void PortHud_SetHidden(int hidden);
int PortSpectator_Mode(void);
int PortSpectator_SetMode(int mode);
int PortSpectator_CanPreview(void);
void PortSpectator_SetTarget(int actor);
int PortSpectator_Target(void);
int PortSpectator_Count(void);
void PortSpectator_SetSpeed(int percent);
int PortSpectator_Speed(void);
void PortSpectator_ResetCamera(void);
void PortSpectator_SetCutOwner(int actor);
int PortSpectator_GetLook(float *out);
void PortSpectator_GetAnchor(float *out);
void PortSpectator_ResolveEye(const float *eye);
void PortSpectator_StageAvoid(const float *targets,int count,const float *look,float *eye);
void PortSpectator_SetScene(const float *bodies,int count,const float *effects,int effectCount,int phase,float projectionX,float projectionY);
void PortSpectator_NoteEffect(int obj);
void PortSpectator_ResetSmoothing(void);
int PortSpectator_FilterPose(float *eye,const float *look,float *rotation);
int PortSpectator_Camera(const float *basePos,const float *baseRot,const float *targets,int count,float *pos,float *rot);
#ifdef __cplusplus
}
#endif
#endif
