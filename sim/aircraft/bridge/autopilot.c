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
                   int airborne=(S.agl>40.0)||(ts-launch_t>10.0);     /* climbed clear of the ground */
                   int stick=(fabs(cr)>0.15||fabs(cp)>0.15||fabs(cy)>0.15);
                   static double last_input=-100; if(stick&&link_up) last_input=ts;
                   int manual=link_up&&(ts-last_input<2.0);
                   rc[4]=2000; rc[5]=2000;                            /* ARM + ANGLE (iNav stabilises) */
                   if(!airborne){ g_mode=ST_CLIMB; rc[1]=1650; rc[2]=1000+(int)(P.climb_thr*1000); }  /* hand-launch climb-out */
                   else if(manual){ g_mode=ST_MANUAL;                 /* operator has the sticks */
                                    rc[0]=1500+(int)(cr*450); rc[1]=1500+(int)(cp*450); rc[3]=1500+(int)(cy*450);
                                    double thr=(cthr>=0)?cthr:0.70; rc[2]=1000+(int)(thr*1000); }
                   else {
                       /* Autonomous mission (default, and on RC-loss): the bridge is the outer
                        * nav loop (companion computer) — iNav ANGLE holds the commanded attitude,
                        * the real FDM + wind + turbulence do the rest. Classic autopilot split:
                        *   pitch  -> hold altitude (g_loalt), damped by climb rate
                        *   throttle-> hold airspeed (cruise), + a bit while climbing
                        *   roll   -> orbit home at g_lorad: coordinated-turn feed-forward + a
                        *             carrot-on-the-circle heading correction. */
                       const double CRUISE_V=P.cruise;
                       double x=(S.lon-HOME_LON)*111320.0*cos(HOME_LAT*RAD), y=(S.lat-HOME_LAT)*111320.0;
                       static int climbing=1;                         /* hysteresis: climb to alt-12, hold until it sinks past alt-60 */
                       if(climbing && S.agl > g_loalt-12) climbing=0;
                       else if(!climbing && S.agl < g_loalt-60) climbing=1;
                       /* On RC-loss the MODE is RTH (climb-to-altitude is an RTH phase,
                        * exactly like real iNav failsafe); with the link up it's the
                        * autonomous climb/loiter mission. */
                       g_mode = !link_up ? ST_RTH : (climbing ? ST_CLIMB : ST_LOITER);
                       double pitchT, thr;
                       if(climbing){ /* full power, steady climb pitch, backed off near stall speed */
                              thr=P.climb_thr; pitchT=P.climb_pitch;
                              if(S.speed<P.stall) pitchT = P.climb_pitch - 3.0*(P.stall-S.speed);   /* stall protection */
                              if(pitchT<0)pitchT=0; if(pitchT>P.climb_pitch+2) pitchT=P.climb_pitch+2; }
                       else { static double alt_i=0; double aerr=g_loalt-S.agl;
                              alt_i+=aerr*0.0004; if(alt_i>3)alt_i=3; if(alt_i<-3)alt_i=-3; /* slow trim, anti-windup */
                              pitchT = 0.10*aerr - 1.3*S.vy + alt_i;                /* altitude hold (P + rate + I) */
                              if(pitchT>10)pitchT=10; if(pitchT<-14)pitchT=-14;    /* -14 deg ~= 4.1 m/s sink at 17 m/s, beats a live thermal */
                              thr = 0.5 + 0.08*(CRUISE_V-S.speed); if(thr>0.9)thr=0.9; if(thr<0.3)thr=0.3; }
                       /* Vector-field loiter (UAV standard): build a desired ground-track that
                        * spirals ONTO the circle of radius g_lorad. The equilibrium radius IS
                        * g_lorad by construction — not an accident of the bank limit — so it flies
                        * the true wide orbit instead of settling into a tight circle near home.
                        *   pos rel. home (x east, y north); radial-out r^, tangent t^ (CCW);
                        *   corr in [-1,1] from the radius error blends tangent<->radial:
                        *     outside (d>R): head inward,  inside (d<R): head outward. */
                       double d=hypot(x,y); if(d<1)d=1;
                       double re=x/d, rn=y/d;                                 /* radial out (unit) */
                       /* steer the GROUND TRACK (heading + wind), not the heading, so the actual
                        * path is the circle and there's no steady crab bias in wind (expert P7). */
                       double yawr=S.yaw*RAD;
                       double gE=S.speed*sin(yawr)+ATM.windE+ATM.gustE, gN=S.speed*cos(yawr)+ATM.windN+ATM.gustN;
                       double gsp=hypot(gE,gN); if(gsp<0.5)gsp=0.5;
                       double te0=gE/gsp, tn0=gN/gsp;                         /* ground-track unit */
                       double track=atan2(gE,gN)*DEG; if(track<0)track+=360;
                       /* Orbit sense = sign of radial x track. When the plane flies nearly RADIALLY
                        * (climb-out, or steering out toward the circle) this is ~0 and its raw sign
                        * chatters -> the roll command flips -> "rolled right while turning left" jerks.
                        * LATCH it and only flip on a clearly-reversed track (hysteresis). */
                       static double odir=0; double crs=re*tn0-rn*te0;
                       if(odir==0) odir=(crs>=0)?1.0:-1.0;
                       else if(crs<-0.45) odir=-1.0; else if(crs>0.45) odir=1.0;   /* wider hysteresis */
                       double dir=odir;
                       double te=-rn*dir, tn=re*dir;                          /* tangent in that direction */
                       /* Desired course = tangent rotated toward the radial by an angle set by the
                        * radius error: inside -> steer OUT (up to ~90°), outside -> steer IN. */
                       double ang=atan2(d-g_lorad, g_lorad*0.5);
                       double cc=cos(ang), sc=sin(ang);
                       double vde=cc*te - sc*re, vdn=cc*tn - sc*rn;
                       double course=atan2(vde,vdn)*DEG;
                       double herr=course-track; while(herr>180)herr-=360; while(herr<-180)herr+=360;
                       double bank_ff=atan(S.speed*S.speed/(fmax(g_lorad,50.0)*9.81))*DEG; /* steady orbit bank */
                       /* Flying nearly RADIALLY (climb-out, steering outward) makes the track-based
                        * course ill-conditioned; gate the heading correction down so a noisy course
                        * can't slam the bank. The steady orbit bank still applies. */
                       double gate=fmin(1.0, fabs(crs)/0.30);
                       double rollT=-dir*bank_ff + 0.22*herr*gate;
                       double rlim=climbing?P.bank_climb:P.bank_cruise;
                       if(rollT>rlim)rollT=rlim; if(rollT<-rlim)rollT=-rlim;
                       /* Slew-limit the commanded bank: no guidance discontinuity (dir flip, mode
                        * change, gust) can make iNav snap the roll — this kills the ~200 deg/s roll
                        * kicks. Max ~18 deg/s of commanded-bank change. */
                       static double rollT_p=0; double mx=18.0*(dt*2.0);
                       if(rollT>rollT_p+mx)rollT=rollT_p+mx; else if(rollT<rollT_p-mx)rollT=rollT_p-mx;
                       rollT_p=rollT;
                       rc[0]=1500+(int)(rollT/30.0*500);   /* ANGLE: full stick ~ iNav max bank 30 deg */
                       rc[1]=1500+(int)(pitchT/30.0*500);
                       rc[2]=1000+(int)(thr*1000);
                   }
                 }
            uint8_t pl[16]; for(int i=0;i<8;i++){ pl[i*2]=rc[i]&0xff; pl[i*2+1]=rc[i]>>8; }
            msp1(200,pl,16);
        }
}
