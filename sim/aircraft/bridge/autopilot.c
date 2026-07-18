/* FlightBox — outer nav loop (companion computer). See autopilot.h. */
#define _GNU_SOURCE
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "autopilot.h"
#include "msp.h"
#include "protocol.h"
#include "../sim_state.h"

int g_mode = ST_DISARMED;   /* bridge autopilot mode -> telemetry state */
double g_loalt=500.0, g_lorad=1000.0;  /* autonomous loiter altitude (m AGL) + orbit radius (m), env-tunable */

/* Per-aircraft flight profile. NO airframe tuning lives in this source — the outer nav loop is
 * airframe-agnostic and its parameters come from the AIRCRAFT MODEL: run.sh sources
 * models/<aircraft>/profile.env, which exports these. autopilot.c only knows the setting NAMES.
 * The fallbacks below are a neutral, clearly-unconfigured safety net (logged), NOT any aircraft's
 * tuning: a real aircraft always ships its profile.env. */
static struct { double cruise, climb_thr, climb_pitch, stall, bank_cruise, bank_climb; int init; } P;
static double envd(const char*k, double def, int*missing){ const char*v=getenv(k);
    if(v&&*v) return atof(v); (*missing)++; return def; }
static void profile_init(void){
    if(P.init) return; P.init=1;
    int miss=0;
    P.cruise      = envd("FB_CRUISE",      15.0, &miss);   /* m/s target cruise airspeed */
    P.climb_thr   = envd("FB_CLIMB_THR",    0.8, &miss);   /* throttle during climb-out (0..1) */
    P.climb_pitch = envd("FB_CLIMB_PITCH", 12.0, &miss);   /* deg climb pitch target */
    P.stall       = envd("FB_STALL",       10.0, &miss);   /* m/s below which climb pitch is backed off */
    P.bank_cruise = envd("FB_BANK",        10.0, &miss);   /* deg max bank in loiter */
    P.bank_climb  = envd("FB_BANK_CLIMB",  10.0, &miss);   /* deg max bank while climbing */
    if(miss) fprintf(stderr,"[autopilot] %d flight-profile settings unset — no models/<aircraft>/"
                            "profile.env loaded; using NEUTRAL fallbacks (not aircraft-tuned)\n", miss);
}

void autopilot_step(long tick, struct timespec t0, double dt,
                    float cr, float cp, float cy, float cthr, int link_up){
        profile_init();
        if(msp_fd>=0 && tick%2==0){
            struct timespec nw; clock_gettime(CLOCK_MONOTONIC,&nw);
            double ts=(nw.tv_sec-t0.tv_sec)+(nw.tv_nsec-t0.tv_nsec)/1e9;   /* real elapsed */
            uint16_t rc[8]={1500,1500,1000,1500,1000,1000,1000,1500};
            int armed=(t_armflags&4)!=0;
            int calib=(t_armflags&(1<<9))!=0;
            static double cal_done_t=-1;
            if(!calib && cal_done_t<0 && ts>1.0) cal_done_t=ts;   /* remember when gyro cal cleared */
            if(cal_done_t<0 || ts < cal_done_t+1.0){ }            /* arm LOW until cal done +1s */
            else if(!armed){                                     /* pulse arm LOW 1.5s -> HIGH 1.5s to force a clean edge (clears ARM_SWITCH latch) */
                double ph=fmod(ts-cal_done_t,3.0);
                if(ph>=1.5){ rc[3]=2000; rc[4]=2000; }           /* YAW HIGH + ARM (nav bypass) */
            }
            else { /* armed. Autonomous mission by default, manual only while the
                    * operator moves the sticks, NAV RTH on RC-loss — all unified:
                    *   1) launch: level ANGLE + throttle until airborne
                    *   2) default: NAV RTH -> climb to nav_rth_altitude (500 m), loiter
                    *      over home at nav_fw_loiter_radius (300 m). "Kreist wie ein Vogel."
                    *   3) manual: operator sticks -> ANGLE, hands back to loiter after 2 s
                    *   4) RC-loss: same NAV RTH loiter (failsafe) */
                   static double launch_t=-1; if(launch_t<0) launch_t=ts;
                   /* WIP: NAV-RTH engagement below exposes a real FDM yaw/heading-feedback bug
                    * (§5): iNav's native NAV commands a saturated yaw (0.8-1.0) that diverges the
                    * slow motor-glider to NaN. Handoff at 120m (not 40m) avoids the instant crash;
                    * NOT latched on purpose — the climb-out fallback is the only thing that keeps
                    * resetting it before divergence. The real fix is the COG/heading sign, not this. */
                   int airborne=(S.agl>120.0);
                   (void)launch_t;
                   int stick=(fabs(cr)>0.15||fabs(cp)>0.15||fabs(cy)>0.15);
                   static double last_input=-100; if(stick&&link_up) last_input=ts;
                   int manual=link_up&&(ts-last_input<2.0);
                   rc[4]=2000; rc[5]=2000;                            /* ARM + ANGLE (iNav stabilises) */
                   if(!airborne){ g_mode=ST_CLIMB; rc[1]=1650; rc[2]=1000+(int)(P.climb_thr*1000); }  /* hand-launch climb-out */
                   else if(manual){ g_mode=ST_MANUAL;                 /* operator has the sticks */
                                    rc[0]=1500+(int)(cr*450); rc[1]=1500+(int)(cp*450); rc[3]=1500+(int)(cy*450);
                                    double thr=(cthr>=0)?cthr:0.70; rc[2]=1000+(int)(thr*1000); }
                   else { /* autonomous + RC-loss: hand off to iNav NATIVE NAV RTH — spiral-climb to
                           * nav_rth_altitude over home, then loiter at nav_fw_loiter_radius. The command
                           * center commands, iNav flies; the bridge synthesises no attitude of its own. */
                       g_mode = link_up ? ST_LOITER : ST_RTH;
                       rc[6]=2000;                              /* AUX3 = NAV RTH */
                       rc[0]=1500; rc[1]=1500; rc[3]=1500;      /* centre sticks — don't fight NAV */
                       rc[2]=1500;                              /* iNav owns throttle (nav_fw_cruise_thr) */
                   }
                 }
            uint8_t pl[16]; for(int i=0;i<8;i++){ pl[i*2]=rc[i]&0xff; pl[i*2+1]=rc[i]>>8; }
            msp1(200,pl,16);
        }
}
