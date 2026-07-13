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
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

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
    double speed;                 /* m/s */
    double vx, vy, vz;            /* X-Plane local: +x east, +y up, +z south */
    /* control inputs from iNav (its mixer outputs) */
    double in_roll, in_pitch, in_yaw, in_thr;
    int armed_hint;               /* set once iNav commands throttle/servo */
} state_t;

static state_t S;
static const double HOME_LAT = 52.045, HOME_LON = 9.385, HOME_ELEV = 300.0;

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
    if(!strcmp(d,"sim/flightmodel/position/P"))      return (float)S.p;
    if(!strcmp(d,"sim/flightmodel/position/Q"))      return (float)S.q;
    if(!strcmp(d,"sim/flightmodel/position/R"))      return (float)S.r;
    if(!strcmp(d,"sim/flightmodel/forces/g_axil"))   return 0.0f;
    if(!strcmp(d,"sim/flightmodel/forces/g_side"))   return 0.0f;
    if(!strcmp(d,"sim/flightmodel/forces/g_nrml"))   return 1.0f;   /* 1 g normal */
    if(!strcmp(d,"sim/flightmodel/position/latitude"))  return (float)S.lat;
    if(!strcmp(d,"sim/flightmodel/position/longitude")) return (float)S.lon;
    if(!strcmp(d,"sim/flightmodel/position/elevation")) return (float)S.elev;
    if(!strcmp(d,"sim/flightmodel/position/y_agl"))     return (float)S.agl;
    if(!strcmp(d,"sim/flightmodel/position/local_vx"))  return (float)S.vx;
    if(!strcmp(d,"sim/flightmodel/position/local_vy"))  return (float)S.vy;
    if(!strcmp(d,"sim/flightmodel/position/local_vz"))  return (float)S.vz;
    if(!strcmp(d,"sim/flightmodel/position/groundspeed"))   return (float)S.speed;
    if(!strcmp(d,"sim/flightmodel/position/true_airspeed")) return (float)S.speed;
    if(!strcmp(d,"sim/weather/barometer_current_inhg")) return (float)baro_inhg(S.elev);
    if(!strcmp(d,"sim/joystick/has_joystick")) return 1.0f;
    if(!strcmp(d,"inav_xitl/plugin/xitlDrefVersion")) return 0.0f; /* plain X-Plane, not XITL */
    return 0.0f;
}

/* --- RC flying-wing presets (real models, spread of weights) --- */
typedef struct { const char *name; double m, b, S, Tmax; } fdm_model_t;
static const fdm_model_t MODELS[] = {
    /* name                 mass    span   area   maxThrust
       kg      m      m^2    N      */
    {"ZOHD-Dart-250G",      0.25,  0.57,  0.10,   4.0},   /* nano wing */
    {"Sonicmodell-AR-Wing", 0.75,  0.90,  0.22,   9.0},   /* popular FPV wing */
    {"Skywalker-X8",        1.90,  2.12,  0.80,  22.0},   /* large FPV/UAV wing */
    {"Skywalker-X8-heavy",  3.40,  2.12,  0.80,  32.0},   /* X8 at max AUW */
};
#define NMODELS ((int)(sizeof(MODELS)/sizeof(MODELS[0])))
static const fdm_model_t *MDL = &MODELS[1];   /* default: AR-Wing */

static int g_inject = 0;   /* XP_INJECT: force a fixed attitude to prove injection */

/* Simplified but realistic flying-wing aerodynamics: lift/drag, pitch static
 * stability + elevon + pitch damping, roll authority + roll damping. Naturally
 * stable so iNav's ANGLE loop has a real plant to control. Scales with the
 * selected preset's mass/span/area. */
static void physics_step(double dt){
    if(g_inject){ S.roll=25.0; S.pitch=-15.0; S.yaw=90.0; return; }
    /* Pre-launch: held level & perfectly still on the hand/launcher so the gyro
     * calibration completes and iNav can arm. Throttle-up = launch. */
    if(S.in_thr < 0.05){ S.roll=0; S.pitch=0; S.p=S.q=S.r=0; return; }
    if(S.speed < 3.0) S.speed = 12.0;      /* hand-launch throw speed */
    const double rho=1.225, g=9.81;
    double m=MDL->m, b=MDL->b, Sw=MDL->S, c=Sw/b;
    double Ix=0.020*m*b*b, Iy=0.030*m*b*b;     /* moments of inertia (est.) */
    const double CLa=4.2, CD0=0.03, kInd=0.06;
    const double Cm0=0.020, Cma=-0.35, Cmde=1.1, Cmq=-9.0;/* pitch: reflex trim, static stab, elevon, damping */
    const double Clda=0.16, Clp=-0.55;         /* roll: elevon diff, damping */

    double V=fmax(S.speed,3.0), Q=0.5*rho*V*V;
    double gamma=(V>0.1)?asin(fmax(-1,fmin(1,S.vy/V))):0.0;   /* flight-path angle, rad */
    double alpha=S.pitch*RAD - gamma;                        /* angle of attack, rad */
    double CL=CLa*alpha, lift=Q*Sw*CL;
    double drag=Q*Sw*(CD0+kInd*CL*CL);
    double T=MDL->Tmax*S.in_thr;

    /* rotational: moments -> angular accel -> rates (deg/s) -> attitude */
    double Cl=Clda*S.in_roll + Clp*(S.p*RAD*b/(2*V));
    double Cm=Cm0 + Cma*alpha + Cmde*S.in_pitch + Cmq*(S.q*RAD*c/(2*V));
    S.p += ((Q*Sw*b*Cl)/Ix)*DEG*dt;
    S.q += ((Q*Sw*c*Cm)/Iy)*DEG*dt;
    S.roll  += S.p*dt;
    S.pitch += S.q*dt;
    if(S.roll>180)S.roll-=360; if(S.roll<-180)S.roll+=360;
    if(S.pitch>85){S.pitch=85;S.q=0;} if(S.pitch<-85){S.pitch=-85;S.q=0;}

    /* translational */
    gamma += ((lift - m*g*cos(gamma))/(m*V))*dt;
    double Vdot=(T-drag)/m - g*sin(gamma);
    S.speed += Vdot*dt; if(S.speed<0)S.speed=0;
    double yaw_rate=(S.speed>2.0)?(g*tan(S.roll*RAD)/S.speed)*DEG:0.0;
    yaw_rate += S.in_yaw*40.0;
    S.r=yaw_rate;
    S.yaw=fmod(S.yaw+yaw_rate*dt+540.0,360.0)-180.0;

    /* vertical + position */
    double climb=S.speed*sin(gamma);
    S.elev+=climb*dt; S.agl+=climb*dt; if(S.agl<0)S.agl=0;
    double vN=S.speed*cos(S.yaw*RAD), vE=S.speed*sin(S.yaw*RAD);
    S.lat+=(vN*dt)/111320.0; S.lon+=(vE*dt)/(111320.0*cos(S.lat*RAD));
    S.vx=vE; S.vy=climb; S.vz=-vN;
}

int main(void){
    int port = getenv("XP_LISTEN_PORT")?atoi(getenv("XP_LISTEN_PORT")):49000;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in me = {0}; me.sin_family=AF_INET; me.sin_addr.s_addr=INADDR_ANY; me.sin_port=htons(port);
    if(bind(fd,(struct sockaddr*)&me,sizeof me)<0){ perror("bind"); return 1; }
    int fl=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK);

    if(getenv("FDM_MODEL")){ int idx=atoi(getenv("FDM_MODEL")); if(idx>=0&&idx<NMODELS) MDL=&MODELS[idx]; }
    S.lat=HOME_LAT; S.lon=HOME_LON; S.elev=HOME_ELEV; S.agl=HOME_ELEV; S.yaw=0;
    S.speed=14.0; S.vy=0;              /* launched at cruise */
    if(getenv("XP_INJECT")) g_inject=1;
    fprintf(stderr,"[xp_bridge] FDM model = %s (m=%.2fkg b=%.2fm S=%.2fm2)\n", MDL->name, MDL->m, MDL->b, MDL->S);

    struct sockaddr_in inav={0}; socklen_t il=0; int have_client=0;
    fprintf(stderr,"[xp_bridge] X-Plane FDM listening on :%d\n",port);

    const double dt=0.01;                 /* 100 Hz */
    long tick=0; int rref_seen=0, dref_seen=0;
    for(;;){
        /* drain incoming iNav packets */
        uint8_t buf[1200]; struct sockaddr_in src; socklen_t sl=sizeof src; ssize_t n;
        while((n=recvfrom(fd,buf,sizeof buf,0,(struct sockaddr*)&src,&sl))>0){
            if(n>=5 && !memcmp(buf,"RREF",4)){
                if(!have_client){ inav=src; il=sl; have_client=1; }
                int32_t id; memcpy(&id,buf+9,4);
                char dref[96]; snprintf(dref,sizeof dref,"%s",(char*)buf+13);
                add_sub(id,dref); rref_seen++;
            } else if(n>=9 && !memcmp(buf,"DREF",4)){
                float val; memcpy(&val,buf+5,4);
                char *dref=(char*)buf+9; /* space-padded, null-terminated */
                dref_seen++;
                if(strstr(dref,"yoke_roll_ratio")) S.in_roll=val;
                else if(strstr(dref,"yoke_pitch_ratio")) S.in_pitch=val;
                else if(strstr(dref,"yoke_heading_ratio")) S.in_yaw=val;
                else if(strstr(dref,"throttle_ratio_all")) { S.in_thr=val; if(val>0.02) S.armed_hint=1; }
            }
        }

        physics_step(dt);

        /* stream all subscribed sensor datarefs back to iNav */
        if(have_client && nsubs>0){
            uint8_t out[5+128*8]; memcpy(out,"RREF",4); out[4]=0; int o=5;
            for(int i=0;i<nsubs;i++){
                int32_t id=subs[i].id; float v=sensor_value(subs[i].dref);
                memcpy(out+o,&id,4); memcpy(out+o+4,&v,4); o+=8;
            }
            sendto(fd,out,o,0,(struct sockaddr*)&inav,il);
        }

        if(++tick % 100 == 0){   /* ~1 Hz status */
            fprintf(stderr,"[xp_bridge] subs=%d rref=%d dref=%d | roll=%.1f pitch=%.1f yaw=%.1f alt=%.0f spd=%.1f | in(r=%.2f p=%.2f y=%.2f t=%.2f) armed=%d\n",
                nsubs,rref_seen,dref_seen,S.roll,S.pitch,S.yaw,S.elev,S.speed,S.in_roll,S.in_pitch,S.in_yaw,S.in_thr,S.armed_hint);
        }
        struct timespec ts={0,(long)(dt*1e9)}; nanosleep(&ts,NULL);
    }
    return 0;
}
