#ifndef GS_GENERIC_PAD_H
#define GS_GENERIC_PAD_H
/* Unmapped, standard dual-stick USB pads still need to reach the binding UI.
 * Per-device mappings leave SDL's known controller database untouched. */
static int PortInput_MapGenericPad(SDL_JoystickID id) {
    SDL_JoystickType type;
    SDL_Joystick *joy;
    int axes,buttons,hats,ok;
    if (SDL_IsGamepad(id)) return 0;
    type=SDL_GetJoystickTypeForID(id);
    if (type!=SDL_JOYSTICK_TYPE_UNKNOWN && type!=SDL_JOYSTICK_TYPE_GAMEPAD) return 0;
    joy=SDL_OpenJoystick(id);
    if (!joy) return 0;
    axes=SDL_GetNumJoystickAxes(joy);
    buttons=SDL_GetNumJoystickButtons(joy);
    hats=SDL_GetNumJoystickHats(joy);
    /* Common PS2-style USB layout. Other layouts can provide a standard
     * gamecontrollerdb.txt mapping; their bindings remain configurable in F1. */
    ok=axes==4 && buttons>=12 && buttons<=16 && hats==1 &&
       SDL_SetGamepadMapping(id,
         "none,Generic USB Dual Stick,a:b2,b:b1,x:b3,y:b0,back:b8,start:b9,"
         "leftstick:b10,rightstick:b11,leftshoulder:b4,rightshoulder:b5,"
         "lefttrigger:b6,righttrigger:b7,leftx:a0,lefty:a1,rightx:a2,righty:a3,"
         "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,");
    SDL_CloseJoystick(joy);
    return ok;
}
#endif
