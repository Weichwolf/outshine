/* FlightBox — outer nav loop (companion computer). See autopilot.h. */
#define _GNU_SOURCE
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "autopilot.h"
#include "msp.h"
#include "protocol.h"
#include "../sim_state.h"

int g_mode = ST_DISARMED;   /* bridge autopilot mode -> telemetry state */

/* Mission waypoints from the harness/CC: MISSION_WPS = "lat,lon,alt_asl;..." (alt already ASL, the
 * CC did AGL->ASL via the DEM). Uploaded once to iNav via MSP_SET_WP (209); iNav then flies NAV WP.
 * g_have_mission gates the autonomous mode: mission present -> NAV WP, else -> NAV RTH (loiter home). */
int g_have_mission = 0;
static void upload_mission_wps(void) {
    const char *s = getenv("MISSION_WPS");
    if (!s || !*s) return;
    double la[48], lo[48], al[48];
    int n = 0;
    const char *p = s;
    while (*p && n < 48) {
        if (sscanf(p, "%lf,%lf,%lf", &la[n], &lo[n], &al[n]) == 3) n++;
        const char *semi = strchr(p, ';');
        if (!semi) break;
        p = semi + 1;
    }
    for (int i = 0; i < n; i++) {
        uint8_t b[21];
        int32_t lat = (int32_t)(la[i] * 1e7), lon = (int32_t)(lo[i] * 1e7), alt = (int32_t)(al[i] * 100);
        b[0] = i + 1;                 /* wp_no (1-based) */
        b[1] = 1;                     /* NAV_WP_ACTION_WAYPOINT */
        memcpy(b + 2, &lat, 4);
        memcpy(b + 6, &lon, 4);
        memcpy(b + 10, &alt, 4);      /* cm, ASL */
        memset(b + 14, 0, 6);         /* p1/p2/p3 */
        b[20] = (i == n - 1) ? 0xa5 : 0;   /* flag: last waypoint */
        msp1(209, b, 21);             /* MSP_SET_WP */
    }
    g_have_mission = (n > 0);
    fprintf(stderr, "[xp_bridge] mission: %d waypoints uploaded via MSP_SET_WP\n", n);
}
double g_loalt=500.0, g_lorad=1000.0;  /* autonomous loiter altitude (m AGL) + orbit radius (m), env-tunable */

/* Per-aircraft flight profile. NO airframe tuning lives in this source — the outer nav loop is
 * airframe-agnostic and its parameters come from the AIRCRAFT MODEL: run.sh sources
 * models/<aircraft>/profile.env, which exports these. autopilot.c only knows the setting NAMES.
 * The fallbacks below are a neutral, clearly-unconfigured safety net (logged), NOT any aircraft's
 * tuning: a real aircraft always ships its profile.env. */
static struct { double cruise, climb_thr, climb_pitch, stall, bank_cruise, bank_climb, vr; int init; } P;
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
    P.vr          = envd("FB_VR",           0.0, &miss);   /* rotation speed (m/s): ground-roll level to Vr,
                                                            * then rotate. 0 = hand-launch (rotate immediately) */
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
                   /* Launch climb-out (ANGLE) -> hand to NAV at 120m, LATCHED (one-way): NAV owns the
                    * aircraft from here, no bounce back to the climb-out. (Safe now that the rudder
                    * sign is fixed; while it was inverted, latching let NAV's yaw runaway diverge.) */
                   static int airborne=0; if(S.agl>120.0) airborne=1;
                   (void)launch_t;
                   static int wp_done=0;               /* upload the mission once, airborne with a GPS fix */
                   if(airborne && !wp_done){ upload_mission_wps(); wp_done=1; }
                   int stick=(fabs(cr)>0.15||fabs(cp)>0.15||fabs(cy)>0.15);
                   static double last_input=-100; if(stick&&link_up) last_input=ts;
                   int manual=link_up&&(ts-last_input<2.0);
                   rc[4]=2000; rc[5]=2000;                            /* ARM + ANGLE (iNav stabilises) */
                   if(!airborne){ g_mode=ST_CLIMB;                    /* takeoff: ground-roll level to Vr, then rotate */
                                  rc[2]=1000+(int)(P.climb_thr*1000);
                                  int rot=1500+(int)(P.climb_pitch/30.0*500);  /* rotate to per-aircraft climb pitch */
                                  rc[1]=(S.speed < P.vr) ? 1500 : rot; }       /* Vr=0 (hand-launch) -> rotate immediately */
                   else if(manual){ g_mode=ST_MANUAL;                 /* operator has the sticks */
                                    rc[0]=1500+(int)(cr*450); rc[1]=1500+(int)(cp*450); rc[3]=1500+(int)(cy*450);
                                    double thr=(cthr>=0)?cthr:0.70; rc[2]=1000+(int)(thr*1000); }
                   else { /* autonomous: iNav flies natively. Mission present -> NAV WP (fly the uploaded
                           * waypoints); else -> NAV RTH (spiral-climb + loiter over home / RC-loss failsafe).
                           * The command center commands, iNav flies; the bridge synthesises no attitude. */
                       if(g_have_mission){ g_mode=ST_LOITER; rc[7]=2000; }   /* AUX4 = NAV WP */
                       else { g_mode = link_up ? ST_LOITER : ST_RTH; rc[6]=2000; }  /* AUX3 = NAV RTH */
                       rc[0]=1500; rc[1]=1500; rc[3]=1500;      /* centre sticks — don't fight NAV */
                       rc[2]=1500;                              /* iNav owns throttle (nav_fw_cruise_thr) */
                   }
                 }
            uint8_t pl[16]; for(int i=0;i<8;i++){ pl[i*2]=rc[i]&0xff; pl[i*2+1]=rc[i]>>8; }
            msp1(200,pl,16);
        }
}
