/* xpjsb/actuators — see actuators.h. */
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "constants.h"
#include "actuators.h"
#include "../fdm/jsbsim_adapter.h"
#include "../bridge/msp.h"
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

static float ap_roll=0, ap_pitch=0, ap_yaw=0;    /* slew-limited surface positions (real servo dynamics) */
static float servo_slew=0;                        /* per-model via env SERVO_SLEW; fast jets want a quick servo */
static float slew(float pos, float cmd){
    float s = servo_slew, d = cmd - pos;
    return pos + (d > s ? s : (d < -s ? -s : d));
}
void xpjsb_actuators_apply(double dt){
    (void)dt;                                    /* throttle slew lives in the adapter (ESC spin-up) */
    if(servo_slew<=0){ const char*e=getenv("SERVO_SLEW"); servo_slew=e?(float)atof(e):XPJSB_SERVO_SLEW; if(servo_slew<=0)servo_slew=XPJSB_SERVO_SLEW; }
    ap_roll=slew(ap_roll,in_roll); ap_pitch=slew(ap_pitch,in_pitch); ap_yaw=slew(ap_yaw,in_yaw);
    S.in_roll=ap_roll; S.in_pitch=ap_pitch; S.in_yaw=ap_yaw; S.in_thr=in_thr;   /* mirror for the flight log */
    fb_jsbsim_set_controls(ap_roll, ap_pitch, ap_yaw, in_thr);
}

long xpjsb_actuators_bad_count(void){ return bad; }

/* --- auxiliary actuators: iNav servo (MSP_SERVO) -> FDM gear/flap/speedbrake, mapped by AUXMAP env --- */
static int aux_gear = -1, aux_flap = -1, aux_sb = -1;   /* servo index per control, -1 = not mapped */
static int aux_ready = 0;

static void aux_init(void){
    aux_ready = 1;
    const char *m = getenv("AUXMAP");
    if(!m || !*m) return;
    char buf[128]; strncpy(buf, m, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for(char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")){
        char *c = strchr(tok, ':'); if(!c) continue; *c = 0; int idx = atoi(c + 1);
        if(idx < 0 || idx >= 16) continue;
        if     (!strcmp(tok,"gear"))       aux_gear = idx;
        else if(!strcmp(tok,"flap"))       aux_flap = idx;
        else if(!strcmp(tok,"speedbrake")) aux_sb   = idx;
    }
}

static double pwm01(int us){ double v = (us - XPJSB_PWM_MIN) / (double)(XPJSB_PWM_MAX - XPJSB_PWM_MIN); return v < 0 ? 0 : (v > 1 ? 1 : v); }

void xpjsb_aux_apply(void){
    if(!aux_ready) aux_init();
    if(aux_gear < 0 && aux_flap < 0 && aux_sb < 0) return;     /* model maps no aux -> leave FDM defaults */
    double gear = aux_gear >= 0 ? 1.0 - pwm01(t_servo[aux_gear]) : -1.0;   /* inverted: un-commanded = DOWN */
    double flap = aux_flap >= 0 ?       pwm01(t_servo[aux_flap]) : -1.0;
    double sb   = aux_sb   >= 0 ?       pwm01(t_servo[aux_sb])   : -1.0;
    fb_jsbsim_set_aux(gear, flap, sb);
}
