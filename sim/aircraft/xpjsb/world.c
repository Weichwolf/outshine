/* xpjsb/world — see world.h. Owns the shared flight-state globals (sim_state.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "constants.h"
#include "world.h"
#include "../fdm/jsbsim_adapter.h"
#include "../terrain.h"
#include "../sim_state.h"

/* --- the shared globals every bridge module reads; this TU owns them --- */
state_t S;
double  HOME_LAT = 52.045, HOME_LON = 9.385, HOME_ELEV = 71.0, HOME_HDG = 0.0, HOME_SPD = 14.0, HOME_ALT = 2.0;
double  HOME_CLEAR_DOWN = 2.0, HOME_CLEAR_UP = 0.5;   /* model gear-down / gear-up CG-above-ground (m), set at init */
fb_atmo ATM;
float   g_nx = 0.0f, g_ny = 0.0f, g_nz = 1.0f;
int     g_crashed = 0;

static double envd(const char *k, double d){ const char *e=getenv(k); return e?atof(e):d; }

int world_init(void){
    if(getenv("ORIGIN_LAT")) HOME_LAT=atof(getenv("ORIGIN_LAT"));
    if(getenv("ORIGIN_LON")) HOME_LON=atof(getenv("ORIGIN_LON"));

    fb_atmo_init(&ATM);                                  /* deterministic by default (E2E: WIND/TURB=0) */
    { double wsp=envd("WIND_SPEED",0.0), wdir=envd("WIND_DIR",240.0), turb=envd("TURB",0.0);
      fb_atmo_set_target(&ATM, -wsp*cos(wdir*RAD), -wsp*sin(wdir*RAD), turb, 0.6*turb, ATM.bl_height, 0.0);
      fb_atmo_snap(&ATM); }

    const char *ta=getenv("TILES_ADDR"); if(!ta) ta="fb-tiles:8081";
    double he;
    if(!fb_terrain_lookup_deadline(ta, HOME_LAT, HOME_LON, &he, 30.0)){
        fprintf(stderr,"[xpjsb] FATAL: no exact ground elevation from fb-tiles — refusing to start\n");
        return 1;
    }
    HOME_ELEV=he;
    fprintf(stderr,"[xpjsb] home ground elevation %.2f m (exact, fb-tiles)\n", HOME_ELEV);
    fb_terrain_start(ta, HOME_ELEV);

    const char *spawn_alt_env = getenv("SPAWN_ALT");   /* explicit only for an airborne spawn (e.g. gliders) */
    HOME_ALT = spawn_alt_env ? atof(spawn_alt_env) : 2.0;   /* provisional; replaced by the model gear height below */
    S.lat=HOME_LAT; S.lon=HOME_LON; S.elev=HOME_ELEV+HOME_ALT; S.agl=HOME_ALT;
    HOME_HDG=envd("ORIGIN_HDG", 0.0);
    S.yaw=HOME_HDG; S.speed=14.0; S.gs=14.0;                           /* spawn aligned with the runway */

    const char *ac=getenv("AIRCRAFT"); if(!ac||!*ac) ac="c172";
    const char *mr=getenv("MODELS_ROOT"); if(!mr||!*mr) mr="/app/models";
    int fbw = (int)envd("FBW", 0);
    double spawn_spd = envd("SPAWN_SPEED", 14.0); HOME_SPD = spawn_spd;
    /* hoff = −1 -> JSBSim sits the aircraft on its gear (model geometry); explicit SPAWN_ALT spawns airborne. */
    double hoff = spawn_alt_env ? atof(spawn_alt_env) : -1.0;
    if(fb_jsbsim_init(mr, ac, HOME_LAT, HOME_LON, HOME_ELEV, hoff, spawn_spd, envd("ORIGIN_HDG",0.0), fbw)!=0){
        fprintf(stderr,"[xpjsb] FATAL: JSBSim init failed for '%s'\n", ac);
        return 1;
    }
    /* HOME_ALT = the model's gear-down CG height above ground, not a fixed 2 m — the held/parked camera eye
     * and the takeoff/touchdown reference are geometry-true, and match JSBSim's on-gear IC (no arm jump).
     * gear-up clearance is the belly line for gear-up touchdown/crash. */
    HOME_CLEAR_DOWN = fb_jsbsim_ground_clearance(1);
    HOME_CLEAR_UP   = fb_jsbsim_ground_clearance(0);
    HOME_ALT = spawn_alt_env ? atof(spawn_alt_env) : (HOME_CLEAR_DOWN > 0.1 ? HOME_CLEAR_DOWN : 2.0);
    S.elev=HOME_ELEV+HOME_ALT; S.agl=HOME_ALT;
    fprintf(stderr,"[xpjsb] FDM=JSBSim aircraft=%s (fbw_override=%d) spawn=%.1f m/s, ground clearance gear-down %.2f m / gear-up %.2f m -> spawn AGL %.2f m\n",
            ac, fbw, spawn_spd, HOME_CLEAR_DOWN, HOME_CLEAR_UP, HOME_ALT);
    return 0;
}

void world_step(double dt, int armed){
    (void)dt;
    if(!armed){
        /* held: level, still, on the ground — iNav's accel-cal + arming preconditions met, but the
         * untrimmed airframe is NOT advanced (it would depart before iNav could catch it). */
        S.roll=0; S.pitch=0; S.yaw=HOME_HDG; S.p=S.q=S.r=0; g_nx=0; g_ny=0; g_nz=1.0f;
        S.speed=HOME_SPD; S.gs=HOME_SPD; S.vx=0; S.vy=0; S.vz=0;   /* report spawn speed while held so iNav's
                                          speed state matches the FDM at arm (a 0->spawn jump departed unstable airframes) */
        S.lat=HOME_LAT; S.lon=HOME_LON; S.elev=HOME_ELEV+HOME_ALT; S.agl=HOME_ALT;
        fb_terrain_set_pos(S.lat, S.lon);
        return;
    }
    fb_jsbsim_set_controls(S.in_roll, S.in_pitch, S.in_yaw, S.in_thr<0?0:S.in_thr);
    double g = fb_terrain_ground();
    fb_jsbsim_set_ground(g);
    fb_jsbsim_set_wind(ATM.windN, ATM.windE);
    fb_fdm_state st; fb_jsbsim_step(&st);
    S.roll=st.roll; S.pitch=st.pitch; S.yaw=st.yaw;
    S.p=st.p; S.q=st.q; S.r=st.r;
    S.lat=st.lat; S.lon=st.lon; S.elev=st.elev;
    S.speed=st.speed; S.gs=st.gs;
    S.vx=st.vx; S.vy=st.vy; S.vz=st.vz;
    g_nx=(float)st.nx; g_ny=(float)st.ny; g_nz=(float)st.nz;
    fb_terrain_set_pos(S.lat, S.lon);
    double agl_true = S.elev - g;
    if(agl_true < 0.0) g_crashed = 1;                     /* below ground while flying = crash; flag, never hide */
    S.agl = agl_true < 0 ? 0 : agl_true;
    if(!isfinite(S.lat)||fabs(S.lat-HOME_LAT)>2) S.lat=HOME_LAT;
    if(!isfinite(S.lon)||fabs(S.lon-HOME_LON)>2) S.lon=HOME_LON;
    if(!isfinite(S.agl)) S.agl=100;
}
