/* xpjsb/actuators — see actuators.h. */
#include <string.h>
#include <math.h>
#include "constants.h"
#include "actuators.h"
#include "../fdm/jsbsim_adapter.h"
#include "../sim_state.h"

static float in_roll = 0, in_pitch = 0, in_yaw = 0, in_thr = 0;
static long  bad = 0;

int xpjsb_actuator_dref(const char *d, float v){
    if(!isfinite(v)) { bad++; return 1; }
    if(strstr(d,"yoke_roll_ratio"))    { if(fabsf(v)<=XPJSB_YOKE_MAX_ABS) in_roll=v;  else bad++; return 1; }
    if(strstr(d,"yoke_pitch_ratio"))   { if(fabsf(v)<=XPJSB_YOKE_MAX_ABS) in_pitch=v; else bad++; return 1; }
    if(strstr(d,"yoke_heading_ratio")) { if(fabsf(v)<=XPJSB_YOKE_MAX_ABS) in_yaw=v;   else bad++; return 1; }
    if(strstr(d,"throttle_ratio_all")) { if(v>=XPJSB_THR_MIN && v<=XPJSB_THR_MAX) in_thr=v; else bad++; return 1; }
    return 0;
}

void xpjsb_actuators_apply(double dt){
    (void)dt;                                    /* throttle slew lives in the adapter (ESC spin-up) */
    S.in_roll=in_roll; S.in_pitch=in_pitch; S.in_yaw=in_yaw; S.in_thr=in_thr;   /* mirror for the flight log */
    fb_jsbsim_set_controls(in_roll, in_pitch, in_yaw, in_thr);
}

long xpjsb_actuators_bad_count(void){ return bad; }
