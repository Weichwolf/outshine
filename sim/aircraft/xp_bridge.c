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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG (180.0/M_PI)
#define RAD (M_PI/180.0)

/* --- physics state (this IS the flight dynamics model) --- */
typedef struct {
    double roll, pitch, yaw;      /* deg, X-Plane phi/theta/psi */
    double p, q, r;               /* deg/s body rates */
    double lat, lon, elev, agl;   /* deg, deg, m, m */
    double speed;                 /* airspeed, m/s (aero uses this) */
    double gs;                    /* groundspeed, m/s (airspeed + wind; GPS reports this) */
    double vx, vy, vz;            /* X-Plane local ground velocity: +x east, +y up, +z south */
    /* control inputs from iNav (its mixer outputs) */
    double in_roll, in_pitch, in_yaw, in_thr;
    int armed_hint;               /* set once iNav commands throttle/servo */
} state_t;

static state_t S;
/* Home / ENU origin. Overridable via ORIGIN_LAT/ORIGIN_LON env so the whole
 * system (aircraft home + command-center osmmesh origin) can fly anywhere. */
static double HOME_LAT = 52.045, HOME_LON = 9.385, HOME_ELEV = 71.0;  /* ground ASL, matches osmmesh terrain at origin */

/* --- atmosphere: steady wind + gusts + turbulence (env-tunable, live-overridable) --- */
static double windN=0, windE=0;          /* steady wind vector, m/s (ground frame) */
static double gustN=0, gustE=0;          /* slowly-varying gust offset, m/s */
static double g_turb=1.0;                /* turbulence intensity (0=calm, 1=moderate, 2=rough) */
static double g_sigma=0.6;               /* Dryden vertical-gust std, m/s (live-set from real gusts) */
static double g_bl_height=800.0;         /* boundary-layer / thermal-top height, m AGL (live) */
static double g_thermal_W=0.0;           /* peak thermal updraft, m/s (0=off; live from solar+BL) */
static double g_gustP=0,g_gustQ=0;       /* current gust roll/pitch rates (deg/s) -> reported to iNav's gyro */
static float  g_nz=1.0f;                  /* normal load factor (g) -> iNav accelerometer, not hardwired 1 */
static float  g_cloud=0.25f, g_vis=30000.0f;  /* live cloud cover 0..1 + horizontal visibility, m */
/* live sun/moon ephemeris (filled ~1 Hz in main from real UTC + origin) */
static float  g_sun_el=45, g_sun_az=180, g_moon_el=-10, g_moon_az=0, g_moon_ph=0.5f;
/* xorshift PRNG + ~gaussian (sum of uniforms). Deterministic per run. */
static uint32_t g_rng=2463534242u;
static double urand(void){ g_rng^=g_rng<<13; g_rng^=g_rng>>17; g_rng^=g_rng<<5; return g_rng/4294967296.0; }
/* ~gaussian, mean 0, UNIT variance (sum of 12 uniforms - 6). The old sum-of-4 had variance
 * 1/3, so every turbulence RMS driven by it was 0.58x the commanded sigma. */
static double nrand(void){ double s=0; for(int i=0;i<12;i++)s+=urand(); return s-6.0; }

/* --- thermals: a field of Gaussian updraft columns on a jittered grid. Strength
 * and top come from live weather (solar radiation + boundary-layer height). The
 * plane bobs through lift and gentle inter-thermal sink, like a real soaring day. */
static uint32_t hash2(int a,int b){ uint32_t h=(uint32_t)(a*73856093)^(uint32_t)(b*19349663); h^=h>>13; h*=0x5bd1e995u; h^=h>>15; return h; }
static double thermal_w(double x,double y,double z){
    if(g_thermal_W<0.1) return 0.0;
    const double cell=340.0, R=110.0;                 /* thermal spacing + core radius, m */
    double zf=z/fmax(g_bl_height,200.0);
    if(zf<=0.0||zf>=1.0) return -0.10*g_thermal_W;     /* below/above the convective layer: no lift */
    double prof=sin(zf*M_PI);                          /* 0 at ground, peak mid-layer, 0 at BL top */
    int gx=(int)floor(x/cell+0.5), gy=(int)floor(y/cell+0.5);
    double best=0.0;
    for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++){
        uint32_t h=hash2(gx+dx,gy+dy);
        double cx=(gx+dx)*cell + ((h&255)/255.0-0.5)*cell*0.6;
        double cy=(gy+dy)*cell + (((h>>8)&255)/255.0-0.5)*cell*0.6;
        double rr=hypot(x-cx,y-cy);
        double str=g_thermal_W*(0.55+0.45*(((h>>16)&255)/255.0));
        double w=str*exp(-(rr*rr)/(R*R));
        if(w>best)best=w;
    }
    return best*prof - 0.12*g_thermal_W;               /* core lift minus gentle ambient sink */
}

/* --- low-precision solar & lunar position (from real UTC + origin lat/lon) --- */
static double julian_day(time_t t){ return t/86400.0 + 2440587.5; }
static void sun_pos(double jd,double lat,double lon,double*el,double*az){
    double n=jd-2451545.0;
    double L=fmod(280.460+0.9856474*n,360.0); if(L<0)L+=360;
    double g=fmod(357.528+0.9856003*n,360.0)*RAD;
    double lam=(L+1.915*sin(g)+0.020*sin(2*g))*RAD;
    double eps=(23.439-4.0e-7*n)*RAD;
    double ra=atan2(cos(eps)*sin(lam),cos(lam));
    double dec=asin(sin(eps)*sin(lam));
    double gmst=fmod(280.46061837+360.98564736629*n,360.0);
    double lst=fmod(gmst+lon,360.0)*RAD, H=lst-ra;
    double sl=sin(lat*RAD), cl=cos(lat*RAD);
    *el=asin(sl*sin(dec)+cl*cos(dec)*cos(H))*DEG;
    double a=atan2(-cos(dec)*sin(H), sin(dec)*cl - cos(dec)*sl*cos(H));
    *az=fmod(a*DEG+360.0,360.0);
}
/* simplified lunar position (Schlyter) + illuminated fraction; visual accuracy only */
static void moon_pos(double jd,double lat,double lon,double*el,double*az,double*illum){
    double d=jd-2451545.0;
    double N=(125.1228-0.0529538083*d)*RAD, inc=5.1454*RAD, w=(318.0634+0.1643573223*d)*RAD;
    double a=60.2666, e=0.054900, M=fmod(115.3654+13.0649929509*d,360.0)*RAD;
    double E=M+e*sin(M)*(1+e*cos(M)); for(int k=0;k<3;k++) E=E-(E-e*sin(E)-M)/(1-e*cos(E));
    double xv=a*(cos(E)-e), yv=a*sqrt(1-e*e)*sin(E);
    double v=atan2(yv,xv), r=hypot(xv,yv);
    double xh=r*(cos(N)*cos(v+w)-sin(N)*sin(v+w)*cos(inc));
    double yh=r*(sin(N)*cos(v+w)+cos(N)*sin(v+w)*cos(inc));
    double zh=r*(sin(v+w)*sin(inc));
    double lon_e=atan2(yh,xh), lat_e=atan2(zh,hypot(xh,yh));
    double ecl=(23.4393-3.563e-7*d)*RAD;
    double xe=cos(lon_e)*cos(lat_e);
    double ye=sin(lon_e)*cos(lat_e)*cos(ecl)-sin(lat_e)*sin(ecl);
    double ze=sin(lon_e)*cos(lat_e)*sin(ecl)+sin(lat_e)*cos(ecl);
    double ra=atan2(ye,xe), dec=atan2(ze,hypot(xe,ye));
    double gmst=fmod(280.46061837+360.98564736629*d,360.0);
    double lst=fmod(gmst+lon,360.0)*RAD, H=lst-ra;
    double sl=sin(lat*RAD), cl=cos(lat*RAD);
    *el=asin(sl*sin(dec)+cl*cos(dec)*cos(H))*DEG;
    double A=atan2(-cos(dec)*sin(H), sin(dec)*cl-cos(dec)*sl*cos(H));
    *az=fmod(A*DEG+360.0,360.0);
    /* illuminated fraction from sun-moon elongation */
    double se,sa; sun_pos(jd,lat,lon,&se,&sa);
    double sunlon=fmod(280.460+0.9856474*(jd-2451545.0),360.0)*RAD;
    double elong=acos(cos(lon_e-sunlon)*cos(lat_e));
    *illum=(1.0-cos(elong))/2.0;
}

/* --- live weather via Open-Meteo (background thread; never blocks the FDM) --- */
static double jnum(const char*s,const char*key){ char p[64]; snprintf(p,sizeof p,"\"%s\":",key);
    const char*q=strstr(s,p); if(!q) return NAN; return atof(q+strlen(p)); }
static double jnum_in(const char*s,const char*sect,const char*key){ const char*b=strstr(s,sect); return jnum(b?b:s,key); }
static double jarr(const char*s,const char*key,int idx){ char p[64]; snprintf(p,sizeof p,"\"%s\":[",key);
    const char*q=strstr(s,p); if(!q) return NAN; q+=strlen(p);
    for(int i=0;i<idx;i++){ q=strchr(q,','); if(!q) return NAN; q++; } return atof(q); }
static void wx_fetch(void){
    char url[640];
    snprintf(url,sizeof url,
      "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
      "&current=wind_speed_10m,wind_direction_10m,wind_gusts_10m,cloud_cover,visibility,temperature_2m"
      "&hourly=boundary_layer_height,shortwave_radiation"
      "&wind_speed_unit=ms&forecast_days=1", HOME_LAT, HOME_LON);
    char cmd[760]; snprintf(cmd,sizeof cmd,"curl -s --max-time 8 \"%s\"",url);
    FILE*f=popen(cmd,"r"); if(!f) return;
    static char buf[32768]; size_t n=fread(buf,1,sizeof buf-1,f); buf[n]=0; pclose(f);
    if(n<40 || !strstr(buf,"wind_speed_10m")) { fprintf(stderr,"[wx] no data (offline?) — keeping current weather\n"); return; }
    double wsp =jnum_in(buf,"\"current\":{","wind_speed_10m");
    double wdir=jnum_in(buf,"\"current\":{","wind_direction_10m");
    double gust=jnum_in(buf,"\"current\":{","wind_gusts_10m");
    double cc  =jnum_in(buf,"\"current\":{","cloud_cover");
    double vis =jnum_in(buf,"\"current\":{","visibility");
    time_t tt=time(NULL); struct tm gm; gmtime_r(&tt,&gm); int hr=gm.tm_hour;
    double blh=jarr(buf,"boundary_layer_height",hr), swr=jarr(buf,"shortwave_radiation",hr);
    if(!isfinite(wsp)||!isfinite(wdir)) { fprintf(stderr,"[wx] parse failed\n"); return; }
    if(!isfinite(gust)) gust=wsp*1.4;
    /* wind at flight altitude ~ a bit stronger than 10 m (log profile); FROM dir -> blows toward */
    double v=wsp*1.25;
    windN=-v*cos(wdir*RAD); windE=-v*sin(wdir*RAD);
    /* turbulence from the gust factor; Dryden sigma scales with it */
    double gf=fmax(0.0,gust-wsp);
    /* realistic low-altitude turbulence: MIL-F-8785C puts sigma_w ~ 0.1*W20; add a
     * modest gust-spread term. Capped so a strong-gust day is rough but still flyable. */
    g_turb=fmax(0.3,fmin(1.6, 0.30+0.13*gf));
    g_sigma=fmin(1.0, 0.08*wsp + 0.11*gf);   /* capped so even a very gusty day stays flyable */
    g_bl_height=isfinite(blh)?fmax(300.0,blh):800.0;
    g_thermal_W=isfinite(swr)?fmax(0.0,fmin(4.5, swr/230.0)):0.0;   /* ~1000 W/m2 midday -> strong lift */
    if(isfinite(cc))  g_cloud=(float)fmax(0.0,fmin(1.0,cc/100.0));
    if(isfinite(vis)) g_vis=(float)fmax(1500.0,fmin(60000.0,vis));
    fprintf(stderr,"[wx] LIVE wind=%.1fm/s@%.0f gust=%.1f cloud=%.0f%% vis=%.0fkm BL=%.0fm sun=%.0fW/m2 -> turb=%.2f therm=%.1f\n",
        wsp,wdir,gust,cc,vis/1000.0,blh,swr,g_turb,g_thermal_W);
}
static int g_wx_live=1;
static void* wx_thread(void*arg){ (void)arg;
    for(;;){ wx_fetch(); sleep(900); }   /* refresh every 15 min */
}

/* dataref subscriptions from iNav: id -> dataref string */
static struct { int id; char dref[96]; } subs[128];
static int nsubs = 0;
static void add_sub(int id, const char *dref){
    for(int i=0;i<nsubs;i++) if(subs[i].id==id){ snprintf(subs[i].dref,sizeof subs[i].dref,"%s",dref); return; }
    if(nsubs<128){ subs[nsubs].id=id; snprintf(subs[nsubs].dref,sizeof subs[nsubs].dref,"%s",dref); nsubs++; }
}

static double baro_inhg(double alt_m){ return 29.92126 * pow(1.0 - 2.25577e-5*alt_m, 5.25588); }

/* value for a subscribed dataref string, from physics */
static float sensor_value(const char *d){
    if(!strcmp(d,"sim/flightmodel/position/phi"))   return (float)S.roll;
    if(!strcmp(d,"sim/flightmodel/position/theta")) return (float)(-S.pitch); /* X-Plane theta sign is opposite iNav pitch */
    if(!strcmp(d,"sim/flightmodel/position/psi"))   return (float)(S.yaw<0?S.yaw+360:S.yaw);
    if(!strcmp(d,"sim/flightmodel/position/hpath"))  return (float)(S.yaw<0?S.yaw+360:S.yaw);
    if(!strcmp(d,"sim/flightmodel/position/P"))      return (float)(S.p+g_gustP);  /* gyro sees the gust rate */
    if(!strcmp(d,"sim/flightmodel/position/Q"))      return (float)(S.q+g_gustQ);
    if(!strcmp(d,"sim/flightmodel/position/R"))      return (float)S.r;
    if(!strcmp(d,"sim/flightmodel/forces/g_axil"))   return 0.0f;
    if(!strcmp(d,"sim/flightmodel/forces/g_side"))   return 0.0f;
    if(!strcmp(d,"sim/flightmodel/forces/g_nrml"))   return g_nz;   /* real normal load factor (turn + gust) */
    if(!strcmp(d,"sim/flightmodel/position/latitude"))  return (float)S.lat;
    if(!strcmp(d,"sim/flightmodel/position/longitude")) return (float)S.lon;
    if(!strcmp(d,"sim/flightmodel/position/elevation")) return (float)S.elev;
    if(!strcmp(d,"sim/flightmodel/position/y_agl"))     return (float)S.agl;
    if(!strcmp(d,"sim/flightmodel/position/local_vx"))  return (float)S.vx;
    if(!strcmp(d,"sim/flightmodel/position/local_vy"))  return (float)S.vy;
    if(!strcmp(d,"sim/flightmodel/position/local_vz"))  return (float)S.vz;
    if(!strcmp(d,"sim/flightmodel/position/groundspeed"))   return (float)S.gs;
    if(!strcmp(d,"sim/flightmodel/position/true_airspeed")) return (float)S.speed;
    if(!strcmp(d,"sim/weather/barometer_current_inhg")) return (float)baro_inhg(S.elev);
    if(!strcmp(d,"sim/joystick/has_joystick")) return 1.0f;
    if(!strcmp(d,"inav_xitl/plugin/xitlDrefVersion")) return 0.0f; /* plain X-Plane, not XITL */
    /* iNav also subscribes to inav_xitl/* datarefs; xplane.c copies numSats/fix
     * into gpsFakeSet regardless of mode, so these MUST be answered for a GPS fix. */
    if(!strcmp(d,"inav_xitl/gps/numSats"))   return 16.0f;
    if(!strcmp(d,"inav_xitl/gps/fix"))       return 2.0f;   /* GPS_FIX_3D == 2 in iNav's enum (NOT 3!). Feeding 3 left STATE(GPS_FIX) unset -> no nav/home/RTH. */
    if(!strcmp(d,"inav_xitl/gps/latitude"))  return (float)S.lat;
    if(!strcmp(d,"inav_xitl/gps/longitude")) return (float)S.lon;
    if(!strcmp(d,"inav_xitl/gps/elevation")) return (float)S.elev;
    if(!strcmp(d,"inav_xitl/gps/groundspeed"))return (float)S.gs;
    if(!strcmp(d,"inav_xitl/sensors/airspeed"))return (float)S.speed;
    if(!strcmp(d,"inav_xitl/sensors/battery_voltage")) return 12.0f;
    if(!strcmp(d,"inav_xitl/sensors/battery_current")) return 1.0f;
    if(!strcmp(d,"inav_xitl/sensors/rangefinder")) return -1.0f;
    if(!strcmp(d,"inav_xitl/rc/rssi"))       return 100.0f;
    if(!strcmp(d,"inav_xitl/rc/failsafe"))   return 0.0f;
    return 0.0f;
}

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

static int g_inject = 0;   /* XP_INJECT: force a fixed attitude to prove injection */
static int g_mode = ST_DISARMED;   /* bridge autopilot mode -> telemetry state */
static double g_loalt=500.0, g_lorad=1000.0;  /* autonomous loiter altitude (m AGL) + orbit radius (m), env-tunable */

/* Simplified but realistic flying-wing aerodynamics: lift/drag, pitch static
 * stability + elevon + pitch damping, roll authority + roll damping. Naturally
 * stable so iNav's ANGLE loop has a real plant to control. Scales with the
 * selected preset's mass/span/area. */
static void physics_step(double dt){
    if(g_inject){ S.roll=25.0; S.pitch=-15.0; S.yaw=90.0; return; }
    /* Pre-launch: held level & perfectly still on the hand/launcher so the gyro
     * calibration completes and iNav can arm. Throttle-up = launch. */
    static int launched=0;
    if(!launched){
        if(S.in_thr < 0.05){ S.roll=0; S.pitch=0; S.p=S.q=S.r=0; S.speed=0; S.vx=S.vy=S.vz=0; return; }
        launched=1; S.speed=12.0;          /* hand-launch throw speed */
    }
    /* once launched, physics always runs (glides at low throttle, e.g. RTH descent) */
    const double rho=1.225, g=9.81;
    double m=MDL->m, b=MDL->b, Sw=MDL->S, c=Sw/b;
    /* Inertias: for a wing, roll (about the long span b) is HEAVIER than pitch (about the
     * short chord c), so Ix > Iy. The old Ix<Iy gave a ~0.024s roll time constant — twitchy
     * and unphysical. Ix=0.06 m b^2 -> Tr~0.12s (realistic), and Ix ~ 2*Iy. */
    double Ix=0.060*m*b*b, Iy=0.030*m*b*b;
    const double CLa=4.2, CD0=0.070, kInd=0.08, CLmax=1.25;   /* draggy FPV wing -> top speed ~24-26 m/s; stalls at CLmax */
    const double Cm0=0.020, Cma=-0.35, Cmde=1.1, Cmq=-9.0;/* pitch: reflex trim, static stab, elevon, damping */
    const double Clda=0.16, Clp=-0.55;         /* roll: elevon diff, damping */

    double V=fmax(S.speed,3.0), Q=0.5*rho*V*V;

    /* --- Dryden-form turbulence FIRST (MIL-F-8785C low-altitude spectra) so the vertical
     * gust can bump the angle of attack below. First-order discrete filters driven by
     * UNIT-variance noise; sigma from the live gust data. Scale lengths grow with altitude
     * (choppy low, smooth high). wg = vertical gust (m/s); pg/qg = gust roll/pitch rates.
     * The MIL scale-length constant is imperial, so Lu is computed in feet. */
    double h=fmax(S.agl,10.0), hf=h/0.3048;
    double Lw=fmax(h,30.0), Lu=0.3048*fmax(hf,50.0)/pow(0.177+0.000823*hf,1.2);
    double aw=fmin(1.0,V*dt/Lw), au=fmin(1.0,V*dt/fmax(Lu,20.0));
    double gsc=fmin(1.0,0.30+0.012*S.agl)*g_sigma;        /* ease near ground for hand-launch climb-out */
    static double wg=0,pg=0,qg=0;
    wg += -aw*wg + gsc     *sqrt(2*aw)*nrand();            /* vertical gust, m/s */
    pg += -au*pg + 3.5*gsc *sqrt(2*au)*nrand();            /* roll-rate gust, deg/s */
    qg += -au*qg + 1.7*gsc *sqrt(2*au)*nrand();            /* pitch-rate gust, deg/s */
    g_gustP=pg; g_gustQ=qg;                                /* export so iNav's gyro (P/Q drefs) sees the turbulence */
    /* thermal updraft column at the current position (air-mass vertical velocity) */
    double tx=(S.lon-HOME_LON)*111320.0*cos(S.lat*RAD), ty=(S.lat-HOME_LAT)*111320.0;
    double w_air=wg + thermal_w(tx,ty,S.agl);             /* total vertical air motion */

    /* --- longitudinal aero ---
     * Aerodynamic (AIR-relative) flight-path angle gamma is a PERSISTENT state integrated
     * from lift — NOT re-derived from ground vy (which carries updraft and would double-count
     * the aircraft's own climb). BUT a vertical gust/thermal tilts the relative wind and
     * TRANSIENTLY raises alpha by ~w/V — the "lift bump" a glider feels flying into a thermal.
     * gamma's own fast relaxation washes it out over the short period, so no nose-high trap. */
    static double gamma=0.0;
    double alpha=S.pitch*RAD - gamma + fmax(-0.10,fmin(0.10, w_air/V));
    /* Stall: lift rises to CLmax then BREAKS (drops) past the stall angle, so the nose falls
     * and speed recovers, with a post-stall drag rise. A real wing loses lift beyond the
     * break — not just saturating flat — which is what makes a stall recover instead of hang. */
    double astall=CLmax/CLa, aabs=fabs(alpha), sgn=(alpha<0?-1.0:1.0), CLlin=CLa*alpha, CLs;
    if(aabs<=astall) CLs=CLlin;
    else CLs=sgn*fmax(0.35, CLmax-0.7*CLa*(aabs-astall));   /* post-stall lift drop */
    double lift=Q*Sw*CLs;
    double dstall=fabs(CLlin-CLs);
    double drag=Q*Sw*(CD0 + kInd*CLs*CLs + 0.9*dstall*dstall);
    double T=MDL->Tmax*S.in_thr;
    g_nz=(float)(lift/(m*g)); if(g_nz<-2)g_nz=-2; if(g_nz>6)g_nz=6;   /* normal load factor -> accelerometer */

    /* --- rotational: moments -> angular accel -> rates (deg/s) -> attitude ---
     * Gusts bodily rotate the airframe (added to attitude kinematics) AND are reported to the
     * gyro above, so iNav's rate loop can damp them — turbulence it can actually fight. */
    double Cl=Clda*S.in_roll + Clp*(S.p*RAD*b/(2*V));
    double Cm=Cm0 + Cma*alpha + Cmde*S.in_pitch + Cmq*(S.q*RAD*c/(2*V));
    S.p += ((Q*Sw*b*Cl)/Ix)*DEG*dt;      /* airframe's own dynamic rates (aero + elevon) */
    S.q += ((Q*Sw*c*Cm)/Iy)*DEG*dt;
    S.roll  += (S.p + pg)*dt;
    S.pitch += (S.q + qg)*dt;
    if(S.roll>180)S.roll-=360; if(S.roll<-180)S.roll+=360;
    if(S.pitch>85){S.pitch=85;S.q=0;} if(S.pitch<-85){S.pitch=-85;S.q=0;}

    /* translational (airspeed dynamics) */
    gamma += ((lift - m*g*cos(gamma))/(m*V))*dt;
    if(gamma>1.4)gamma=1.4; if(gamma<-1.4)gamma=-1.4;        /* keep the air-frame path angle sane */
    double Vdot=(T-drag)/m - g*sin(gamma);
    S.speed += Vdot*dt; if(S.speed<0)S.speed=0;
    double yaw_rate=(S.speed>2.0)?(g*tan(S.roll*RAD)/S.speed)*DEG:0.0;
    yaw_rate += S.in_yaw*40.0;
    S.r=yaw_rate;
    S.yaw=fmod(S.yaw+yaw_rate*dt+540.0,360.0)-180.0;

    /* gusts: Ornstein-Uhlenbeck horizontal wind wander (mean-revert to 0 offset) */
    double gtau=3.0, gsig=0.52*g_turb;   /* RMS tuned for the now unit-variance nrand */
    gustN += (-gustN/gtau)*dt + gsig*sqrt(dt)*nrand();
    gustE += (-gustE/gtau)*dt + gsig*sqrt(dt)*nrand();

    /* air-relative velocity (aircraft flies along heading through the air) + wind
     * = GROUND velocity. Position/GPS use ground velocity; aero used airspeed above.
     * The plane drifts downwind, so iNav must crab to hold a loiter — as in reality. */
    double vN_air=S.speed*cos(S.yaw*RAD), vE_air=S.speed*sin(S.yaw*RAD);
    double vN=vN_air+windN+gustN, vE=vE_air+windE+gustE;
    S.gs=hypot(vN,vE);
    double climb=S.speed*sin(gamma) + w_air;   /* aero climb + air-mass vertical motion (gust + thermal) */
    S.elev+=climb*dt; S.agl+=climb*dt; if(S.agl<0)S.agl=0;
    S.lat+=(vN*dt)/111320.0; S.lon+=(vE*dt)/(111320.0*cos(S.lat*RAD));
    S.vx=vE; S.vy=climb; S.vz=-vN;
    /* robustness: keep the FDM from blowing up at ground/edge cases */
    if(!isfinite(S.speed)||S.speed>60) S.speed=isfinite(S.speed)?60:12;
    if(!isfinite(S.gs)||S.gs>80) S.gs=S.speed;
    if(!isfinite(S.lat)||fabs(S.lat-HOME_LAT)>2) S.lat=HOME_LAT;
    if(!isfinite(S.lon)||fabs(S.lon-HOME_LON)>2) S.lon=HOME_LON;
    if(!isfinite(S.agl)) S.agl=100;
    if(!isfinite(S.roll))S.roll=0; if(!isfinite(S.pitch))S.pitch=0;
}

/* ---- MSP client to iNav (TCP 5760): RC inject + telemetry read ---- */
static int msp_fd=-1;
static uint8_t msp_rx[8192]; static int msp_rxn=0;
static float t_roll=0,t_pitch=0; static int t_yaw=0,t_fix=0,t_sats=0,t_batt10=126;
static int t_inav_dth=0,t_inav_dir=0;   /* iNav's own distance/direction to home (MSP_COMP_GPS) */
static int t_estalt=0;                  /* iNav estimated altitude, cm (MSP_ALTITUDE) */
static double t_inav_lat=0,t_inav_lon=0;
static uint32_t t_armflags=0,t_modeflags=0;
static uint8_t boxids[64]; static int nboxids=0;
static int msp_connect(void){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_port=htons(5760);
    inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);
    if(connect(fd,(struct sockaddr*)&a,sizeof a)<0){ close(fd); return -1; }
    int one=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one);
    int fl=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK);
    return fd;
}
static void msp1(uint8_t cmd,const uint8_t*p,uint8_t n){
    if(msp_fd<0)return; uint8_t b[6+80]; b[0]='$';b[1]='M';b[2]='<';b[3]=n;b[4]=cmd; uint8_t k=n^cmd;
    for(int i=0;i<n;i++){b[5+i]=p[i];k^=p[i];} b[5+n]=k; send(msp_fd,b,6+n,MSG_NOSIGNAL);
}
static uint8_t crc8s(const uint8_t*d,int n){ uint8_t c=0; for(int i=0;i<n;i++){c^=d[i]; for(int j=0;j<8;j++)c=(c&0x80)?((c<<1)^0xD5):(c<<1);} return c; }
static void msp2(uint16_t fn){
    if(msp_fd<0)return; uint8_t b[9]={'$','X','<',0,(uint8_t)(fn&0xff),(uint8_t)(fn>>8),0,0,0}; b[8]=crc8s(b+3,5); send(msp_fd,b,9,MSG_NOSIGNAL);
}
static void msp_poll(void){
    if(msp_fd<0)return; ssize_t n;
    while((n=recv(msp_fd,msp_rx+msp_rxn,sizeof msp_rx-msp_rxn,0))>0){ msp_rxn+=n; if(msp_rxn>(int)sizeof msp_rx-800)msp_rxn=0; }
    int i=0;
    while(i+3<msp_rxn){
        if(msp_rx[i]=='$'&&msp_rx[i+1]=='M'&&msp_rx[i+2]=='>'){
            if(i+5>msp_rxn)break; int ln=msp_rx[i+3],cmd=msp_rx[i+4]; if(i+6+ln>msp_rxn)break; uint8_t*pl=msp_rx+i+5;
            if(cmd==108&&ln>=6){ t_roll=(int16_t)(pl[0]|pl[1]<<8)/10.0f; t_pitch=(int16_t)(pl[2]|pl[3]<<8)/10.0f; t_yaw=(int16_t)(pl[4]|pl[5]<<8); }
            else if(cmd==106&&ln>=2){ t_fix=pl[0]; t_sats=pl[1];
                if(ln>=10){ int32_t la,lo; memcpy(&la,pl+2,4); memcpy(&lo,pl+6,4); t_inav_lat=la/1e7; t_inav_lon=lo/1e7; } }
            else if(cmd==107&&ln>=4){ t_inav_dth=pl[0]|pl[1]<<8; t_inav_dir=pl[2]|pl[3]<<8; }
            else if(cmd==109&&ln>=4){ int32_t a; memcpy(&a,pl,4); t_estalt=a; }   /* est alt cm */
            else if(cmd==110&&ln>=1){ t_batt10=pl[0]; }
            else if(cmd==101&&ln>=10) memcpy(&t_modeflags,pl+6,4);
            else if(cmd==119){ nboxids=ln<64?ln:64; memcpy(boxids,pl,nboxids); }
            i+=6+ln;
        } else if(msp_rx[i]=='$'&&msp_rx[i+1]=='X'&&msp_rx[i+2]=='>'){
            if(i+8>msp_rxn)break; int ln=msp_rx[i+6]|msp_rx[i+7]<<8,fn=msp_rx[i+4]|msp_rx[i+5]<<8; if(i+9+ln>msp_rxn)break;
            if(fn==0x2000&&ln>=13) memcpy(&t_armflags,msp_rx+i+8+9,4);
            i+=9+ln;
        } else i++;
    }
    if(i>0){ memmove(msp_rx,msp_rx+i,msp_rxn-i); msp_rxn-=i; }
}
static int mode_active(int boxid){ for(int k=0;k<nboxids;k++) if(boxids[k]==boxid) return (t_modeflags>>k)&1; return 0; }

/* ---- artificial horizon from attitude -> RGB565 video (protocol.h) ---- */
static void render_horizon(video_packet_t*v,float roll,float pitch){
    v->magic=FB_MAGIC_VIDEO; v->w=VID_W; v->h=VID_H; v->_pad=0;
    float r=roll*(float)RAD, sr=sinf(r), cr=cosf(r);
    float cx=VID_W/2.f, cy=VID_H/2.f, thr=pitch*(VID_H/50.f);
    uint16_t sky=RGB565(70,130,220), gnd=RGB565(60,150,60), line=RGB565(245,245,245);
    for(int y=0;y<VID_H;y++)for(int x=0;x<VID_W;x++){
        float d=((x-cx)*sr+(y-cy)*cr)-thr;
        v->pix[y*VID_W+x]=(fabsf(d)<1.2f)?line:(d<0?sky:gnd);
    }
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
    /* atmosphere: steady wind (WIND_SPEED m/s FROM WIND_DIR deg) + turbulence (TURB) */
    { double wsp=getenv("WIND_SPEED")?atof(getenv("WIND_SPEED")):3.5;
      double wdir=getenv("WIND_DIR")?atof(getenv("WIND_DIR")):240.0;   /* WSW default */
      windN=-wsp*cos(wdir*RAD); windE=-wsp*sin(wdir*RAD);              /* FROM dir -> blows toward */
      if(getenv("TURB")) g_turb=atof(getenv("TURB")); }
    g_sigma=0.6*g_turb;                                                /* default gust std tracks turb */
    if(getenv("THERMAL")) g_thermal_W=atof(getenv("THERMAL"));         /* manual thermal strength override */
    if(getenv("WX_LIVE")) g_wx_live=atoi(getenv("WX_LIVE"));
    if(getenv("LOITER_ALT")) g_loalt=atof(getenv("LOITER_ALT"));
    if(getenv("LOITER_RADIUS")) g_lorad=atof(getenv("LOITER_RADIUS"));
    if(g_lorad<180.0) g_lorad=180.0;   /* below ~V^2/(g*tan(bank cap)) the low-bank loiter can't hold it */
    /* live weather in the background: real wind/gusts/cloud/visibility for the origin.
     * Non-blocking; if the container is offline it just keeps the env defaults. */
    if(g_wx_live){ pthread_t th; if(pthread_create(&th,NULL,wx_thread,NULL)==0) pthread_detach(th); }
    S.lat=HOME_LAT; S.lon=HOME_LON; S.elev=HOME_ELEV; S.agl=1.0; S.yaw=0; S.speed=14.0; S.gs=14.0; S.vy=0;  /* launch from the ground */
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
                if(strstr(dr,"yoke_roll_ratio"))S.in_roll=val; else if(strstr(dr,"yoke_pitch_ratio"))S.in_pitch=val;
                else if(strstr(dr,"yoke_heading_ratio"))S.in_yaw=val; else if(strstr(dr,"throttle_ratio_all"))S.in_thr=val; }
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

        /* --- auto-launch sequence + RC to iNav (senderless: cal -> yaw-bypass arm -> ANGLE -> throttle) --- */
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
                   if(!airborne){ g_mode=ST_CLIMB; rc[1]=1650; rc[2]=1000+(int)(0.95*1000); }  /* hand-launch climb-out */
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
                       const double CRUISE_V=17.0;
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
                              thr=0.95; pitchT=20.0;
                              if(S.speed<14.0) pitchT = 20.0 - 3.0*(14.0-S.speed);   /* stall protection */
                              if(pitchT<0)pitchT=0; if(pitchT>22)pitchT=22; }
                       else { static double alt_i=0; double aerr=g_loalt-S.agl;
                              alt_i+=aerr*0.0004; if(alt_i>3)alt_i=3; if(alt_i<-3)alt_i=-3; /* slow trim, anti-windup */
                              pitchT = 0.10*aerr - 1.3*S.vy + alt_i;                /* altitude hold (P + rate + I) */
                              if(pitchT>10)pitchT=10; if(pitchT<-8)pitchT=-8;
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
                       double gE=S.speed*sin(yawr)+windE+gustE, gN=S.speed*cos(yawr)+windN+gustN;
                       double gsp=hypot(gE,gN); if(gsp<0.5)gsp=0.5;
                       double te0=gE/gsp, tn0=gN/gsp;                         /* ground-track unit */
                       double track=atan2(gE,gN)*DEG; if(track<0)track+=360;
                       /* Orbit sense = sign of radial x track. When the plane flies nearly RADIALLY
                        * (climb-out, or steering out toward the circle) this is ~0 and its raw sign
                        * chatters -> the roll command flips -> "rolled right while turning left" jerks.
                        * LATCH it and only flip on a clearly-reversed track (hysteresis). */
                       static double odir=0; double crs=re*tn0-rn*te0;
                       if(odir==0) odir=(crs>=0)?1.0:-1.0;
                       else if(crs<-0.35) odir=-1.0; else if(crs>0.35) odir=1.0;
                       double dir=odir;
                       double te=-rn*dir, tn=re*dir;                          /* tangent in that direction */
                       /* Desired course = tangent rotated toward the radial by an angle set by the
                        * radius error: inside -> steer OUT (up to ~90°), outside -> steer IN. This
                        * gives a strong outward velocity when far inside so it reaches the wide
                        * circle quickly, fading to a pure tangent (the circle) as d -> g_lorad. */
                       double ang=atan2(d-g_lorad, g_lorad*0.5);
                       double cc=cos(ang), sc=sin(ang);
                       double vde=cc*te - sc*re, vdn=cc*tn - sc*rn;
                       double course=atan2(vde,vdn)*DEG;
                       double herr=course-track; while(herr>180)herr-=360; while(herr<-180)herr+=360;
                       double bank_ff=atan(S.speed*S.speed/(fmax(g_lorad,50.0)*9.81))*DEG; /* steady orbit bank */
                       /* Gentle correction + a LOW bank cap: the cap keeps it from winding into a
                        * tight circle (a 20° bank orbits at ~130 m); ~1.7° holds the true 1000 m. */
                       double rollT=-dir*bank_ff + 0.22*herr;
                       double rlim=climbing?12.0:10.0;
                       if(rollT>rlim)rollT=rlim; if(rollT<-rlim)rollT=-rlim;
                       rc[0]=1500+(int)(rollT/30.0*500);   /* ANGLE: full stick ~ iNav max bank 30 deg */
                       rc[1]=1500+(int)(pitchT/30.0*500);
                       rc[2]=1000+(int)(thr*1000);
                   }
                 }
            uint8_t pl[16]; for(int i=0;i<8;i++){ pl[i*2]=rc[i]&0xff; pl[i*2+1]=rc[i]>>8; }
            msp1(200,pl,16);
        }

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
        if(tick%50==0){ double jd=julian_day(time(NULL)), el,az,mel,maz,mil;
            sun_pos(jd,HOME_LAT,HOME_LON,&el,&az);   g_sun_el=(float)el;  g_sun_az=(float)az;
            moon_pos(jd,HOME_LAT,HOME_LON,&mel,&maz,&mil); g_moon_el=(float)mel; g_moon_az=(float)maz; g_moon_ph=(float)mil; }
        if(1){
            double hd=hypot((S.lat-HOME_LAT)*111320.0,(S.lon-HOME_LON)*111320.0*cos(HOME_LAT*RAD));
            double tohome=atan2f(-(S.lon-HOME_LON),-(S.lat-HOME_LAT))*DEG;
            telem_packet_t t={0}; t.magic=FB_MAGIC_TELEM;
            /* Attitude straight from the FDM (100 Hz) — NOT from iNav's MSP_ATTITUDE, which
             * the bridge only polls at ~5 Hz. That 5 Hz was the "stepped rotation" jitter:
             * position was smooth (100 Hz) but the camera rotated in 5 Hz steps. This is the
             * true airframe attitude anyway (what iNav reads via the X-Plane DREFs). */
            t.roll=(float)S.roll; t.pitch=(float)S.pitch; t.yaw=(float)S.yaw;
            t.alt=(float)S.agl; t.x=(float)((S.lon-HOME_LON)*111320.0); t.y=(float)((S.lat-HOME_LAT)*111320.0);
            t.gs=(float)S.gs; t.batt=(t_batt10>50&&t_batt10<255)?t_batt10/10.0f:11.4f; t.home_dist=(float)hd;
            float hb=(float)(tohome - S.yaw); while(hb>180)hb-=360; while(hb<-180)hb+=360; t.home_bearing=hb;
            float need=(hd>1)?atan2f((float)S.agl,(float)hd)*(float)DEG:0; t.glideslope_err=need-7.0f;
            t.cloud=g_cloud; t.vis=g_vis;
            t.sun_el=g_sun_el; t.sun_az=g_sun_az;
            t.moon_el=g_moon_el; t.moon_az=g_moon_az; t.moon_phase=g_moon_ph;
            t.vs=(float)S.vy; t.airspeed=(float)S.speed;
            int armed=(t_armflags&4)!=0;
            t.state = !armed?ST_DISARMED : g_mode;   /* bridge autopilot mode */
            /* Link quality falls with range like a real FPV downlink (free-space-ish
             * quadratic roll-off to a reference range), with a little flicker; 0 when the
             * operator link is lost (RC failsafe) or disarmed. */
            float lq=0.f;
            if(armed && link_up){ float dr=(float)hd/4500.f; lq=100.f*(1.f-dr*dr)+(float)(nrand()*1.5);
                if(lq>100)lq=100; if(lq<5)lq=5; }
            t.rssi = (uint8_t)lq;
            sendto(fbfd,&t,sizeof t,0,(struct sockaddr*)&fbdst,sizeof fbdst);
        }
        if(tick%8==0){ static video_packet_t v; render_horizon(&v,t_roll,t_pitch); sendto(fbfd,&v,sizeof v,0,(struct sockaddr*)&fbdst,sizeof fbdst); }

        if(++tick%100==0){ const char*MN[]={"DISARM","ARMED","CLIMB","LOITER","MANUAL","RTH"};
            fprintf(stderr,"[xp_bridge] %s alt=%.0f pitch=%.1f roll=%.1f | airspd=%.1f gs=%.1f home=%.0f\n",
            MN[g_mode%6], S.agl, S.pitch, S.roll, S.speed, S.gs,
            hypot((S.lat-HOME_LAT)*111320.0,(S.lon-HOME_LON)*111320.0*cos(HOME_LAT*RAD))); }
        struct timespec tsp={0,(long)(dt*1e9)}; nanosleep(&tsp,NULL);
    }
    return 0;
}
