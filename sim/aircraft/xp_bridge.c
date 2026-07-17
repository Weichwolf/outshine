/* FlightBox — X-Plane FDM bridge for real iNav SITL.
 * Speaks the X-Plane UDP protocol (port 49000) that iNav SITL (--sim=xp) uses:
 *   - iNav sends RREF requests to subscribe to sensor datarefs -> we stream them
 *     from our physics model.
 *   - iNav sends DREF packets with its control outputs (yoke_* + throttle) ->
 *     we feed them into the physics.
 * This replaces our hand-written FBW with the REAL iNav firmware; this program
 * is only the flight-dynamics model + the sim link.
 *
 * Step A1/A2: X-Plane handshake + sensor injection + physics.
 * (MSP RC/telemetry and the flightbox downlink are added next.)
 *
 * Env: XP_LISTEN_PORT (default 49000). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "protocol.h"
#include "terrain.h"
#include "fdm/ephemeris.h"
#include "fdm/atmosphere.h"
#include "fdm/weather.h"
#include "bridge/msp.h"
#include "bridge/xp_link.h"
#include "bridge/telemetry.h"
#include "bridge/autopilot.h"
#include "sim_state.h"

state_t S;
/* Home / ENU origin. Overridable via ORIGIN_LAT/ORIGIN_LON env so the whole
 * system (aircraft home + command-center osmmesh origin) can fly anywhere. */
double HOME_LAT = 52.045, HOME_LON = 9.385, HOME_ELEV = 71.0;  /* ground ASL, matches osmmesh terrain at origin */

/* --- atmosphere: steady wind + gusts + turbulence + thermals. Lives in fdm/atmosphere.c;
 * this is the only handle on it. Env-tunable at start-up, live-overridable by wx_fetch. --- */
fb_atmo ATM;
float  g_nz=1.0f;        /* normal load factor (g) -> iNav accelerometer, not hardwired 1 */
static long   g_bad_dref=0;     /* out-of-range control DREFs rejected (iNav servo glitches) */
/* live sun/moon ephemeris (filled ~1 Hz in main from real UTC + origin) */
float  g_sun_el=45, g_sun_az=180, g_moon_el=-10, g_moon_az=0, g_moon_ph=0.5f;
int g_inject = 0;   /* XP_INJECT: force a fixed attitude to prove injection (read by physics_step) */

/* --- RC flying-wing presets (real models, spread of weights) --- */
typedef struct { const char *name; double m, b, S, Tmax; } fdm_model_t;
static const fdm_model_t MODELS[] = {
    /* name                 mass    span   area   maxThrust
       kg      m      m^2    N   (realistic static thrust, T/W ~0.6-0.9) */
    {"ZOHD-Dart-250G",      0.25,  0.57,  0.10,   2.2},   /* nano wing */
    {"Sonicmodell-AR-Wing", 0.75,  0.90,  0.22,   6.5},   /* popular FPV wing */
    {"Skywalker-X8",        1.90,  2.12,  0.80,  12.0},   /* large FPV/UAV wing */
    {"Skywalker-X8-heavy",  3.40,  2.12,  0.80,  19.0},   /* X8 at max AUW */
};
#define NMODELS ((int)(sizeof(MODELS)/sizeof(MODELS[0])))
static const fdm_model_t *MDL = &MODELS[1];   /* default: AR-Wing */

/* --- Component-based full-envelope aerodynamics (Selig, AIAA J. Aircraft 2015,
 * "Real-Time Flight Simulation of Highly Maneuverable UAVs"). The airframe is a
 * SUM of lifting elements — a strip-theory wing (left/right elevon panels), a
 * vertical fin/keel, plus a pod — each seeing its OWN local relative flow
 * (freestream + body-rate x moment-arm). Forces & moments are summed in the body
 * frame and fed to the 6-DOF rigid-body equations. This is what makes turns
 * PHYSICAL: a bank tilts the wing lift (centripetal force), the resulting
 * sideslip loads the fin, the fin yaws the nose INTO the bank — yaw follows roll
 * through real moments, so a left turn while banked right is unreachable. */

/* Full-envelope thin-surface coefficients: attached linear lift blended into
 * flat-plate theory (Cd~2, Cl~2 sin^2a cosa at post-stall) across the whole
 * +-180 deg range via a smooth sigmoid (Beard/McLain form; matches Selig Fig 5,
 * Eq 7-12 qualitatively). `ang` = element angle to its own relative flow (rad). */
static inline double esat(double x){ return exp(x<-40.0?-40.0:(x>40.0?40.0:x)); }
static void surf_coeffs(double ang,double a_slope,double cd0,double k_ind,
                        double *Cl,double *Cd){
    const double a0=0.24, M=15.0;                       /* stall ~14 deg; blend width */
    double num=1.0+esat(-M*(ang-a0))+esat(M*(ang+a0));
    double den=(1.0+esat(-M*(ang-a0)))*(1.0+esat(M*(ang+a0)));
    double sig=num/den;                                 /* 0 attached -> 1 flat-plate */
    double s=sin(ang), cA=cos(ang), sgn=(ang<0?-1.0:1.0);
    double Cl_att=a_slope*ang;
    double Cl_fp =2.0*sgn*s*s*cA;                       /* flat-plate lift */
    *Cl=(1.0-sig)*Cl_att + sig*Cl_fp;
    double Cd_att=cd0 + k_ind*Cl_att*Cl_att;            /* profile + induced */
    double Cd_fp =cd0 + 2.0*s*s;                        /* flat-plate drag (Cd~2 @ 90) */
    *Cd=(1.0-sig)*Cd_att + sig*Cd_fp;
}

/* Body-axis <-> local NED (North,East,Down) rotations from Euler phi,theta,psi
 * (roll right +, pitch nose-up +, yaw/heading). Standard 3-2-1 DCM. */
static void ned_to_body(double ph,double th,double ps,double N,double E,double D,
                        double*bx,double*by,double*bz){
    double sp=sin(ph),cp=cos(ph),st=sin(th),ct=cos(th),ss=sin(ps),cs=cos(ps);
    *bx=  ct*cs*N + ct*ss*E - st*D;
    *by=(sp*st*cs-cp*ss)*N + (sp*st*ss+cp*cs)*E + sp*ct*D;
    *bz=(cp*st*cs+sp*ss)*N + (cp*st*ss-sp*cs)*E + cp*ct*D;
}
static void body_to_ned(double ph,double th,double ps,double bx,double by,double bz,
                        double*N,double*E,double*D){
    double sp=sin(ph),cp=cos(ph),st=sin(th),ct=cos(th),ss=sin(ps),cs=cos(ps);
    *N= ct*cs*bx + (sp*st*cs-cp*ss)*by + (cp*st*cs+sp*ss)*bz;
    *E= ct*ss*bx + (sp*st*ss+cp*cs)*by + (cp*st*ss-sp*cs)*bz;
    *D=   -st*bx +          sp*ct*by +          cp*ct*bz;
}

static void physics_step(double dt){
    if(g_inject){ S.roll=25.0; S.pitch=-15.0; S.yaw=90.0; return; }
    /* Pre-launch: held level & perfectly still on the hand/launcher so the gyro
     * calibration completes and iNav can arm. Throttle-up = launch. */
    static int launched=0;
    /* body-frame AIR-relative velocity (m/s): u fwd, v right, w down. This is the
     * core 6-DOF translational state (persists across calls). */
    static double u=0,v=0,w=0;
    if(!launched){
        if(S.in_thr < 0.05){ S.roll=0; S.pitch=0; S.p=S.q=S.r=0; S.speed=0;
            u=v=w=0; S.vx=S.vy=S.vz=0; return; }
        launched=1; u=12.0; v=0; w=0; S.speed=12.0;      /* hand-launch throw speed */
    }
    /* once launched, physics always runs (glides at low throttle, e.g. RTH descent) */
    const double rho=1.225, g=9.81;
    double m=MDL->m, b=MDL->b, Sw=MDL->S, c=Sw/b, AR=b*b/Sw;
    /* Inertias for a wing: roll (span-distributed mass) > pitch (short chord),
     * yaw (both) largest. Scaled off the preset so all four presets fly. */
    /* Inertias: pitch (Iy) kept substantial so the elevon can't drive a fast pitch
     * limit-cycle against iNav's ANGLE loop (that would ring the vertical velocity). */
    double Ix=0.055*m*b*b, Iy=0.045*m*b*b, Iz=0.080*m*b*b;
    double a_w=2.0*M_PI*AR/(AR+2.0);       /* finite-wing lift-curve slope, /rad */
    double kind=1.0/(M_PI*0.85*AR);        /* induced-drag factor (e=0.85) */
    const double CD0=0.055;                /* wing profile drag -> top speed ~26 m/s */
    /* control + stability derivatives (per rad unless noted) */
    const double Clda=0.16;                /* elevon differential -> roll authority (bank-led turns) */
    const double Cmde=0.28;                /* elevon symmetric -> pitch authority (nose-up +) */
    const double Cm0 =0.02;                /* reflex camber -> nose-up trim moment */
    const double Cmq =-14.0;               /* heavy pitch-rate damping (well-damped short period) */
    /* Heavy yaw-rate damping: a rudderless wing must fly nearly coordinated, so the
     * yaw rate has to track the slow g*sin(phi)/V turn and REJECT fast gust-induced
     * sideslip. Weak damping let turbulence jitter the yaw past the small coordinated
     * value at low bank (the coord-turn-rate / sign failures). */
    const double Cnr =-0.45;               /* strong yaw-rate damping -> coordinated even at low bank */
    const double Clb =-0.05;               /* dihedral effect (sweep): roll from sideslip */
    const double Cnda= 0.0;                /* no EXPLICIT adverse yaw: it comes intrinsically from the
                                            * strips' differential induced drag during a roll RATE, so it
                                            * vanishes in steady bank (no wrong-sign yaw at small bank). */
    const double Cndr= 0.003;              /* rudderless wing: in_yaw -> a whisper of differential-drag yaw only */
    double xw=-0.10*c;                     /* wing AC behind CG -> static pitch stability */
    /* vertical fin / keel: small area/arm -> gentle weathercock (Cnb~0.04). Kept low
     * so gusts don't over-excite yaw; coordination comes from turn geometry + damping. */
    double lf=0.40*b, Sf=0.06*Sw, zf=-0.10*b, a_f=2.2, kf=0.15, cdf0=0.02;
    const int NS=6; double dS=Sw/NS;       /* wing discretised into 6 spanwise strips */

    double Vrep=fmax(S.speed,3.0);         /* for turbulence scale-lengths below */

    /* Ease the air toward the last live-weather observation (no-op until wx_fetch lands one). */
    fb_atmo_slew(&ATM, dt);
    /* Turbulence + gust wander: the atmosphere module owns the maths and the state. */
    fb_atmo_step_gusts(&ATM, dt, S.agl, Vrep);
    /* thermal + vertical gust = total vertical air motion at the current position */
    double tx=(S.lon-HOME_LON)*111320.0*cos(S.lat*RAD), ty=(S.lat-HOME_LAT)*111320.0;
    double w_air=ATM.wg + fb_atmo_thermal_w(&ATM,tx,ty,S.agl);

    /* Euler state -> radians; body rates -> rad/s (integrated internally, exported deg/s) */
    double phi=S.roll*RAD, th=S.pitch*RAD, psi=S.yaw*RAD;
    double p=S.p*RAD, q=S.q*RAD, r=S.r*RAD;
    double T=MDL->Tmax*S.in_thr;                          /* prop thrust along +x body */

    /* --- elevon SERVO LAG (first-order, ~6 Hz bandwidth). Real servos are not
     * instantaneous; this low-passes iNav's control output so its fast rate loop
     * cannot toggle the elevons every exchange and drive a ~50 Hz (Nyquist) roll
     * limit-cycle against this fast-rolling plant (tau_roll~0.03s). Low-frequency
     * authority — bank/pitch commands, coordination — passes through untouched. */
    static double el_roll=0, el_pitch=0;
    const double tau_srv=0.030, a_srv=dt/(tau_srv+dt);   /* ~5 Hz servo; ~10x attenuation at the 50 Hz Nyquist */
    el_roll  += a_srv*(S.in_roll  - el_roll);
    el_pitch += a_srv*(S.in_pitch - el_pitch);

    /* The fluctuating air velocity (NED) resolved into body axes: subtract it from
     * the aircraft's velocity-through-mean-air to get the true relative flow the
     * surfaces feel. This is how gusts/thermals become real aerodynamic upsets. */
    double gbx,gby,gbz;
    ned_to_body(phi,th,psi, ATM.gustN,ATM.gustE,-w_air, &gbx,&gby,&gbz);

    double Fz_aero=0.0;                                   /* body-z aero force -> load factor */

    /* --- 6-DOF integration, sub-stepped for stability at the fixed 100 Hz dt --- */
    const int NSUB=3; double dts=dt/NSUB;
    for(int it=0; it<NSUB; it++){
        double sph=sin(phi),cph=cos(phi),sth=sin(th),cth=cos(th);
        /* relative-flow freestream (body), gust-perturbed */
        double ua=u-gbx, va=v-gby, wa=w-gbz;
        double V=sqrt(ua*ua+va*va+wa*wa), Vf=fmax(V,3.0);
        double qbar=0.5*rho*Vf*Vf;
        double beta=asin(fmax(-1.0,fmin(1.0,va/Vf)));    /* sideslip */

        double Fx=0,Fy=0,Fz=0, Mx=0,My=0,Mz=0;
        Fz_aero=0.0;
        /* gravity (body frame) + thrust */
        Fx += -m*g*sth;  Fy += m*g*cth*sph;  Fz += m*g*cth*cph;
        Fx += T;

        /* --- WING: strip theory. Each strip sees freestream + (omega x r). The
         * spanwise offset y_i makes roll rate raise/lower the local alpha (intrinsic
         * roll damping) and differential lift/drag produce roll & yaw moments -- the
         * mechanism Selig relies on for coupled rolls. */
        for(int i=0;i<NS;i++){
            double yi=(-0.5*b)+(i+0.5)*(b/NS);           /* strip centre, span station */
            double Vlx=ua + (q*0.0 - r*yi);              /* + (omega x r)_x , z_w=0 */
            double Vlz=wa + (p*yi - q*xw);               /* + (omega x r)_z */
            double Vn=sqrt(fmax(Vlx*Vlx+Vlz*Vlz,1e-4));
            double qN=0.5*rho*Vn*Vn;
            double as=atan2(Vlz,Vlx);                    /* local angle of attack */
            double Cl,Cd; surf_coeffs(as,a_w,CD0,kind,&Cl,&Cd);
            double L=qN*dS*Cl, D=qN*dS*Cd;
            /* drag along -flow, lift perpendicular (up = -z for +alpha), in x-z plane */
            double Fxi=(-D*Vlx + L*Vlz)/Vn;
            double Fzi=(-D*Vlz - L*Vlx)/Vn;
            Fx+=Fxi; Fz+=Fzi; Fz_aero+=Fzi;
            Mx+= yi*Fzi;                                 /* differential lift -> roll (+ damping) */
            My+= -xw*Fzi;                                /* lift x arm-behind-CG -> pitch stability */
            Mz+= -yi*Fxi;                                /* differential drag -> yaw (adverse) */
        }

        /* --- VERTICAL FIN / KEEL: a side-force element behind & above the CG. Its
         * local sideslip beta' loads it; the arm lf turns that into a yawing moment
         * (weathercock + yaw damping via r), the height zf into a rolling moment
         * (dihedral-like). THIS is the yaw<->sideslip<->roll coupling that forbids
         * turning against the bank. */
        double Vfx=ua + q*zf;                            /* + (omega x r)_x at (-lf,0,zf) */
        double Vfy=va + (-r*lf - p*zf);                  /* + (omega x r)_y */
        double Vfn=sqrt(fmax(Vfx*Vfx+Vfy*Vfy,1e-4));
        double qF=0.5*rho*Vfn*Vfn;
        double bf=atan2(Vfy,Vfx);                        /* fin local sideslip */
        double Clf,Cdf; surf_coeffs(bf,a_f,cdf0,kf,&Clf,&Cdf);
        double Ffy=-qF*Sf*Clf;                           /* side force opposing sideslip */
        double Ffx=-qF*Sf*Cdf*Vfx/Vfn;                   /* fin drag */
        Fx+=Ffx; Fy+=Ffy;
        Mx+= -zf*Ffy;                                    /* fin above CG -> stable dihedral effect */
        My+=  zf*Ffx;
        Mz+= -lf*Ffy;                                    /* fin behind CG -> weathercock into the slip */

        /* --- CONTROL & residual derivatives (elevons are rudderless; in_yaw ~ dead).
         * qbar uses the aircraft airspeed (elevons act on the whole wing). */
        Mx += qbar*Sw*b*( Clda*el_roll + Clb*beta );
        My += qbar*Sw*c*( Cm0 + Cmde*el_pitch + Cmq*(q*c/(2.0*Vf)) );
        Mz += qbar*Sw*b*( Cnda*el_roll + Cndr*S.in_yaw + Cnr*(r*b/(2.0*Vf)) );

        /* --- rigid-body equations of motion (body frame) --- */
        double ax=Fx/m, ay=Fy/m, az=Fz/m;
        double du=ax-(q*w - r*v);
        double dv=ay-(r*u - p*w);
        double dw=az-(p*v - q*u);
        double dp=(Mx+(Iy-Iz)*q*r)/Ix;
        double dq=(My+(Iz-Ix)*r*p)/Iy;
        double dr=(Mz+(Ix-Iy)*p*q)/Iz;
        u+=du*dts; v+=dv*dts; w+=dw*dts;
        p+=dp*dts; q+=dq*dts; r+=dr*dts;
        /* Euler kinematics (guard gimbal near +-90 deg pitch) */
        double ctg=cth; if(fabs(ctg)<0.15) ctg=(ctg<0?-0.15:0.15);
        double dphi=p+(q*sph+r*cph)*(sth/ctg);
        double dth = q*cph - r*sph;
        double dpsi=(q*sph+r*cph)/ctg;
        phi+=dphi*dts; th+=dth*dts; psi+=dpsi*dts;
        th=fmax(-1.48,fmin(1.48,th));                    /* ~+-85 deg gimbal guard */
    }

    /* gust roll/pitch buffet bodily rotates the airframe (iNav's gyro already saw it) */
    phi += ATM.pg*RAD*dt;  th += ATM.qg*RAD*dt;
    th=fmax(-1.48,fmin(1.48,th));

    /* --- write attitude / rates back to the shared state --- */
    S.roll=phi*DEG;  while(S.roll>180)S.roll-=360; while(S.roll<-180)S.roll+=360;
    S.pitch=th*DEG;
    S.yaw=fmod(psi*DEG+540.0,360.0)-180.0;
    S.p=p*DEG; S.q=q*DEG; S.r=r*DEG;
    S.speed=sqrt(u*u+v*v+w*w);
    g_nz=(float)(-Fz_aero/(m*g)); if(g_nz<-2)g_nz=-2; if(g_nz>6)g_nz=6;

    /* --- body velocity -> earth frame -> ground velocity (add wind+gust) -> position.
     * The plane drifts downwind, so iNav must crab to hold a loiter — as in reality. */
    double VN,VE,VD;
    body_to_ned(phi,th,psi, u,v,w, &VN,&VE,&VD);
    double vN=VN+ATM.windN+ATM.gustN, vE=VE+ATM.windE+ATM.gustE;
    S.gs=hypot(vN,vE);
    double climb=-VD + w_air;                            /* aero climb + air-mass vertical motion */
    S.elev+=climb*dt;
    /* TRUE AGL: height above the ACTUAL ground, queried from the fb-tiles world-data service.
     * This used to integrate alongside S.elev, i.e. AGL was just (elev - HOME_ELEV) -- the model
     * believed the world was a flat disc at 71 m, so it disagreed with the terrain the renderer
     * drew everywhere except exactly over home. The lookup is cached and polled off-thread; the
     * hot path only reads it. */
    fb_terrain_set_pos(S.lat,S.lon);
    S.agl = S.elev - fb_terrain_ground();
    if(S.agl<0){ S.agl=0; S.elev=fb_terrain_ground(); }   /* ground contact */
    S.lat+=(vN*dt)/111320.0; S.lon+=(vE*dt)/(111320.0*cos(S.lat*RAD));
    S.vx=vE; S.vy=climb; S.vz=-vN;

    /* robustness: keep the FDM from blowing up at ground/edge cases */
    if(!isfinite(S.speed)||S.speed>60){ S.speed=isfinite(S.speed)?60:12; u=S.speed;v=0;w=0; }
    if(!isfinite(S.gs)||S.gs>80) S.gs=S.speed;
    if(!isfinite(S.lat)||fabs(S.lat-HOME_LAT)>2) S.lat=HOME_LAT;
    if(!isfinite(S.lon)||fabs(S.lon-HOME_LON)>2) S.lon=HOME_LON;
    if(!isfinite(S.agl)) S.agl=100;
    if(!isfinite(S.roll)){S.roll=0;} if(!isfinite(S.pitch)){S.pitch=0;}
    if(!isfinite(S.p)){S.p=0;} if(!isfinite(S.q)){S.q=0;} if(!isfinite(S.r)){S.r=0;}
}

/* Deterministic clock for the ephemeris ONLY: SIM_UTC (unix epoch seconds, parsed once) overrides
 * real time so the aircraft's sun/moon and the renderer's stars share one faked "now". 0/unset = live.
 * Telemetry timestamps and the weather clock stay on real time. */
static time_t fb_now(void){
    static time_t sim=-1;
    if(sim==-1){ const char*e=getenv("SIM_UTC"); sim=(e&&atol(e)>0)?(time_t)atol(e):0; }
    return sim>0 ? sim : time(NULL);
}

int main(void){
    int port = getenv("XP_LISTEN_PORT")?atoi(getenv("XP_LISTEN_PORT")):49000;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in me = {0}; me.sin_family=AF_INET; me.sin_addr.s_addr=INADDR_ANY; me.sin_port=htons(port);
    if(bind(fd,(struct sockaddr*)&me,sizeof me)<0){ perror("bind"); return 1; }
    int fl=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK);

    if(getenv("FDM_MODEL")){ int idx=atoi(getenv("FDM_MODEL")); if(idx>=0&&idx<NMODELS) MDL=&MODELS[idx]; }
    if(getenv("ORIGIN_LAT")) HOME_LAT=atof(getenv("ORIGIN_LAT"));
    if(getenv("ORIGIN_LON")) HOME_LON=atof(getenv("ORIGIN_LON"));
    /* atmosphere: steady wind (WIND_SPEED m/s FROM WIND_DIR deg) + turbulence (TURB).
     * fb_atmo_init FIRST: ATM is a static, so without it every default would be 0 — a calm,
     * thermal-free, sigma-0 world, silently different from the old file-scope initialisers. */
    fb_atmo_init(&ATM);
    { double wsp=getenv("WIND_SPEED")?atof(getenv("WIND_SPEED")):3.5;
      double wdir=getenv("WIND_DIR")?atof(getenv("WIND_DIR")):240.0;   /* WSW default */
      double turb=getenv("TURB")?atof(getenv("TURB")):ATM.turb;
      /* Seed target AND current: the env value is the starting weather, not something to
       * ramp toward from zero. wx_fetch overrides both as soon as real weather lands. */
      fb_atmo_set_target(&ATM,
          -wsp*cos(wdir*RAD), -wsp*sin(wdir*RAD),                      /* FROM dir -> blows toward */
          turb,
          0.6*turb,                                                     /* default gust std tracks turb */
          ATM.bl_height,
          getenv("THERMAL")?atof(getenv("THERMAL")):ATM.thermal_W);     /* manual thermal override */
      fb_atmo_snap(&ATM);
      ATM.first = 1; }   /* the env is only a guess: the first LIVE observation still applies at once */
    if(getenv("WX_LIVE")) g_wx_live=atoi(getenv("WX_LIVE"));
    if(getenv("LOITER_ALT")) g_loalt=atof(getenv("LOITER_ALT"));
    if(getenv("LOITER_RADIUS")) g_lorad=atof(getenv("LOITER_RADIUS"));
    if(g_lorad<180.0) g_lorad=180.0;   /* below ~V^2/(g*tan(bank cap)) the low-bank loiter can't hold it */
    /* live weather in the background: real wind/gusts/cloud/visibility for the origin.
     * Non-blocking; if the container is offline it just keeps the env defaults. */
    if(g_wx_live){ pthread_t th; if(pthread_create(&th,NULL,wx_thread,NULL)==0) pthread_detach(th); }
    /* Home's ground elevation comes from real DEM data via fb-tiles (the old hardcoded 71.0 m
     * only ever matched Hameln -- it made any other origin fly underground or in the air). One
     * blocking lookup here is fine; after this the poller keeps it current off-thread. */
    { const char*ta=getenv("TILES_ADDR"); if(!ta)ta="fb-tiles:8081";
      double he;
      if(fb_terrain_lookup(ta,HOME_LAT,HOME_LON,&he)){ HOME_ELEV=he;
          fprintf(stderr,"[xp_bridge] home ground elevation %.1f m (fb-tiles)\n",HOME_ELEV); }
      else fprintf(stderr,"[xp_bridge] fb-tiles unreachable, seeding home elevation %.1f m\n",HOME_ELEV);
      fb_terrain_start(ta,HOME_ELEV); }
    S.lat=HOME_LAT; S.lon=HOME_LON; S.elev=HOME_ELEV+2.0; S.agl=2.0; S.yaw=0; S.speed=14.0; S.gs=14.0; S.vy=0;  /* launch 2 m above the ground */
    if(getenv("XP_INJECT")) g_inject=1;
    fprintf(stderr,"[xp_bridge] FDM=%s  X-Plane :%d  MSP->127.0.0.1:5760\n", MDL->name, port);

    /* flightbox UDP: recv control on FB_UP_PORT, send telem+video to FLIGHTBOX_ADDR:FB_DOWN_PORT */
    int fbfd=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in fbme={0}; fbme.sin_family=AF_INET; fbme.sin_addr.s_addr=INADDR_ANY; fbme.sin_port=htons(FB_UP_PORT);
    bind(fbfd,(struct sockaddr*)&fbme,sizeof fbme); { int f=fcntl(fbfd,F_GETFL,0); fcntl(fbfd,F_SETFL,f|O_NONBLOCK); }
    const char*fbh=getenv("FLIGHTBOX_ADDR"); if(!fbh)fbh="127.0.0.1";
    struct sockaddr_in fbdst={0}; fbdst.sin_family=AF_INET; fbdst.sin_port=htons(FB_DOWN_PORT); inet_pton(AF_INET,"127.0.0.1",&fbdst.sin_addr);
    { struct addrinfo h={0},*r; h.ai_family=AF_INET; h.ai_socktype=SOCK_DGRAM; char ps[8]; snprintf(ps,8,"%d",FB_DOWN_PORT);
      if(getaddrinfo(fbh,ps,&h,&r)==0){ memcpy(&fbdst,r->ai_addr,sizeof fbdst); freeaddrinfo(r); } }

    struct sockaddr_in inav={0}; socklen_t il=0; int have_client=0;
    float cr=0,cp=0,cy=0,cthr=-1;  /* control corrections from flightbox (-1 thr = autonomous) */
    int link_up=1;                 /* operator RC link; 0 = simulate RC-loss -> iNav failsafe */
    const double dt=0.01;
    long tick=0;
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC,&t0);
    for(;;){
        /* --- X-Plane exchange with iNav --- */
        uint8_t buf[1200]; struct sockaddr_in src; socklen_t sl=sizeof src; ssize_t n;
        while((n=recvfrom(fd,buf,sizeof buf,0,(struct sockaddr*)&src,&sl))>0){
            if(n>=5 && !memcmp(buf,"RREF",4)){ if(!have_client){inav=src;il=sl;have_client=1;} int32_t id; memcpy(&id,buf+9,4); char dr[96]; snprintf(dr,sizeof dr,"%s",(char*)buf+13); add_sub(id,dr); }
            else if(n>=9 && !memcmp(buf,"DREF",4)){ float val; memcpy(&val,buf+5,4); char*dr=(char*)buf+9;
                /* VALIDATE. X-Plane yoke ratios are -1..+1 and throttle 0..1 — anything else is a
                 * glitch, not a command. iNav's SITL derives the yoke from the servo PWM as
                 * (servo-1500)/500, so a momentarily-unwritten servo of 0 arrives as EXACTLY -3.0:
                 * three times full deflection, which slammed the roll at up to 224 deg/s ("kicks").
                 * Reject out-of-range samples and hold the last good value (clamping to -1 would
                 * still be full aileron). Only genuine, in-range commands reach the FDM. */
                if(!isfinite(val)) { /* drop */ }
                else if(strstr(dr,"yoke_roll_ratio"))    { if(fabsf(val)<=1.05f) S.in_roll=val;  else g_bad_dref++; }
                else if(strstr(dr,"yoke_pitch_ratio"))   { if(fabsf(val)<=1.05f) S.in_pitch=val; else g_bad_dref++; }
                else if(strstr(dr,"yoke_heading_ratio")) { if(fabsf(val)<=1.05f) S.in_yaw=val;   else g_bad_dref++; }
                else if(strstr(dr,"throttle_ratio_all")) { if(val>=-0.05f&&val<=1.05f) S.in_thr=val; else g_bad_dref++; } }
        }
        physics_step(dt);
        if(have_client && nsubs>0){ uint8_t out[5+128*8]; memcpy(out,"RREF",4); out[4]=0; int o=5;
            for(int i=0;i<nsubs;i++){ int32_t id=subs[i].id; float v=sensor_value(subs[i].dref); memcpy(out+o,&id,4); memcpy(out+o+4,&v,4); o+=8; }
            sendto(fd,out,o,0,(struct sockaddr*)&inav,il); }

        /* --- MSP: connect, poll telemetry ---
         * XP_NOMSP=1: act as a pure X-Plane sensor responder (level & still, GPS fix) and
         * leave the MSP port free. Used by make-eeprom.sh to apply CLI config over TCP 5760. */
        if(!getenv("XP_NOMSP")){
            if(msp_fd<0 && tick%50==0){ msp_fd=msp_connect(); if(msp_fd>=0){ msp1(119,NULL,0); } }
            msp_poll();
        }

        /* --- control uplink from flightbox --- */
        ctrl_packet_t c; struct sockaddr_in cs; socklen_t csl=sizeof cs;
        while(recvfrom(fbfd,&c,sizeof c,0,(struct sockaddr*)&cs,&csl)==(ssize_t)sizeof c)
            if(c.magic==FB_MAGIC_CTRL){ cr=c.roll; cp=c.pitch; cy=c.yaw; cthr=c.throttle; link_up=c.link_up; }

        /* --- auto-launch + autonomous nav loop -> RC to iNav (senderless) --- */
        autopilot_step(tick,t0,dt,cr,cp,cy,cthr,link_up);

        /* --- request telemetry from iNav (round-robin) --- */
        if(tick%20==0) msp1(108,NULL,0);
        if(tick%20==5) msp1(106,NULL,0);
        if(tick%20==10) msp1(110,NULL,0);
        if(tick%20==15){ msp1(101,NULL,0); msp2(0x2000); }
        if(tick%20==18) msp1(107,NULL,0);   /* MSP_COMP_GPS: iNav distance/dir to home */
        if(tick%20==12) msp1(109,NULL,0);   /* MSP_ALTITUDE: iNav estimated altitude */

        /* --- downlink telem + video to flightbox (~20 Hz) --- */
        /* Telemetry = the camera pose, sent every physics tick (~100 Hz). The browser
         * renders at 60 fps and samples the LATEST pose each frame — since the state
         * updates faster than the display (game-engine style), the camera is smooth with
         * zero interpolation latency. Video (artificial horizon fallback) stays at ~12 Hz. */
        if(tick%50==0){ double jd=fb_julian_day(fb_now()), el,az,mel,maz,mil;
            fb_sun_pos(jd,HOME_LAT,HOME_LON,&el,&az);   g_sun_el=(float)el;  g_sun_az=(float)az;
            fb_moon_pos(jd,HOME_LAT,HOME_LON,&mel,&maz,&mil); g_moon_el=(float)mel; g_moon_az=(float)maz; g_moon_ph=(float)mil; }
        telem_send(fbfd,fbdst,link_up);
        if(tick%8==0) telem_video(fbfd,fbdst);

        if(++tick%100==0){ const char*MN[]={"DISARM","ARMED","CLIMB","LOITER","MANUAL","RTH"};
            /* in_* = what iNav ACTUALLY put on the servos, as this bridge received it.
             *
             * This line used to print only the aircraft's ATTITUDE. CLAUDE.md's own process
             * section says why that is not enough, in as many words: "the worst bug in the
             * project's history (iNav emitting a -3.0 yoke ratio = 3x full aileron) was invisible
             * for a long time because only the aircraft's attitude was ever measured, never the
             * COMMAND driving it." The rule was written down and the log still did not follow it.
             * Right now the elevator is suspected of sitting at neutral while the autopilot asks
             * for -8 deg; without in_pitch here that stays an inference forever. */
            fprintf(stderr,"[xp_bridge] %s alt=%.0f pitch=%.1f roll=%.1f | airspd=%.1f gs=%.1f home=%.0f"
                           " | in: p=%+.3f r=%+.3f y=%+.3f thr=%.2f | badDREF=%ld\n",
            MN[g_mode%6], S.agl, S.pitch, S.roll, S.speed, S.gs,
            hypot((S.lat-HOME_LAT)*111320.0,(S.lon-HOME_LON)*111320.0*cos(HOME_LAT*RAD)),
            S.in_pitch, S.in_roll, S.in_yaw, S.in_thr, g_bad_dref); }
        struct timespec tsp={0,(long)(dt*1e9)}; nanosleep(&tsp,NULL);
    }
    return 0;
}
