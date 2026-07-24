/* msp_bridge — the World inside the aircraft container.
 *
 * Owns JSBSim (the physics = "the real world") and speaks iNav's built-in HITL
 * protocol MSP_SIMULATOR (0x201F, MSPv2) to a VANILLA iNav-SITL over localhost
 * TCP: every step it pushes the simulated sensors (GPS, attitude/IMU, baro) and
 * applies iNav's actuator reply (roll/pitch/yaw/throttle) back into the physics.
 *
 * No autopilot, no navigation here. iNav flies natively; the Command Center
 * commands it over the SEPARATE MSP radio link. This process is the airframe +
 * its sensors/actuators — nothing more. On real hardware it is replaced by the
 * physical aircraft; the MSP radio seam to the Command Center is unchanged.
 *
 * Sensor fidelity: default feeds truth attitude (iNav trusts it directly); set
 * FB_HITL_IMU=1 to instead feed raw acc+gyro+mag and let iNav's AHRS fuse the
 * attitude itself (closer to a real FC, but iNav marks it experimental). */
#include "FGFDMExec.h"
#include "initialization/FGInitialCondition.h"
#include "initialization/FGTrim.h"
#include "models/FGPropulsion.h"
#include "models/propulsion/FGEngine.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>

using namespace JSBSim;

static const double FT = 0.3048, R2D = 57.29577951308232, D2R = 1.0/57.29577951308232;
static const double MS2KT = 1.9438444924406;

/* ---- MSP ---------------------------------------------------------------- */
enum { MSP_SIMULATOR = 0x201F };
enum { HITL_ENABLE = 1<<0, HITL_USE_IMU = 1<<3, HITL_HAS_NEW_GPS_DATA = 1<<4 };

static uint8_t crc8(uint8_t c, uint8_t a){ c^=a; for(int i=0;i<8;i++) c=(c&0x80)?(c<<1)^0xD5:c<<1; return c; }

static int msp_connect(const char *host, int port){
    struct addrinfo hint={}, *res; hint.ai_family=AF_INET; hint.ai_socktype=SOCK_STREAM;
    char ps[8]; snprintf(ps,sizeof ps,"%d",port);
    for(int tries=0; tries<200; tries++){                 /* SITL may still be coming up */
        if(getaddrinfo(host,ps,&hint,&res)==0){
            int fd=socket(AF_INET,SOCK_STREAM,0);
            if(connect(fd,res->ai_addr,res->ai_addrlen)==0){
                int one=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one);  /* kill Nagle: HITL is request/reply */
                freeaddrinfo(res); return fd; }
            close(fd); freeaddrinfo(res);
        }
        usleep(50000);
    }
    return -1;
}

static int msp_send(int fd, uint16_t cmd, const uint8_t *p, uint16_t n){
    uint8_t f[9+512]; int m=0;                            /* one contiguous frame -> one write() */
    f[m++]='$'; f[m++]='X'; f[m++]='<'; f[m++]=0;
    f[m++]=cmd; f[m++]=cmd>>8; f[m++]=n; f[m++]=n>>8;
    uint8_t crc=0; for(int i=3;i<8;i++) crc=crc8(crc,f[i]);
    for(int i=0;i<n;i++){ f[m++]=p[i]; crc=crc8(crc,p[i]); }
    f[m++]=crc;
    return write(fd,f,m)==m ? 0 : -1;
}

/* read one MSPv2 reply payload; returns size or <0 */
static int msp_recv(int fd, uint16_t *cmd, uint8_t *buf, int cap){
    uint8_t b; int st=0; uint16_t n=0, got=0;
    for(int g=0; g<1000000; g++){
        if(read(fd,&b,1)!=1) return -1;
        switch(st){
            case 0: st=(b=='$')?1:0; break;
            case 1: st=(b=='X')?2:0; break;
            case 2: st=(b=='>'||b=='!')?3:0; break;
            case 3: st=4; break;                          /* flag */
            case 4: *cmd=b; st=5; break;
            case 5: *cmd|=b<<8; st=6; break;
            case 6: n=b; st=7; break;
            case 7: n|=b<<8; got=0; if(n>cap) return -2; st=n?8:9; break;
            case 8: buf[got++]=b; if(got==n) st=9; break;
            case 9: return n;                             /* crc; trust it over loopback */
        }
    }
    return -1;
}

static void w8 (uint8_t*p,int&o,uint8_t v){ p[o++]=v; }
static void w16(uint8_t*p,int&o,int v){ p[o++]=v; p[o++]=v>>8; }
static void w32(uint8_t*p,int&o,long v){ p[o++]=v; p[o++]=v>>8; p[o++]=v>>16; p[o++]=v>>24; }

/* ---- JSBSim ------------------------------------------------------------- */
static FGFDMExec *fdm=nullptr;
static double thr_applied=0.0;
static const double ESC_SPINUP_S=0.5;

static double envd(const char*k, double d){ const char*e=getenv(k); return e?atof(e):d; }
static int    envi(const char*k, int d){ const char*e=getenv(k); return e?atoi(e):d; }

int main(){
    const char *host  = getenv("FB_SITL_HOST"); if(!host) host="127.0.0.1";
    int         port  = envi("FB_SITL_PORT", 5760);
    const char *models= getenv("FB_MODELS_DIR"); if(!models) models="/app/models";
    const char *ac    = getenv("AIRCRAFT"); if(!ac){ fprintf(stderr,"[msp_bridge] AIRCRAFT unset\n"); return 2; }
    double dt         = envd("FB_DT", 0.01);
    double lat        = envd("FB_LAT", 52.1), lon = envd("FB_LON", 9.36);
    double gnd        = envd("FB_GND_ELEV", 0.0);          /* runway MSL, m */
    double hoff       = envd("FB_START_AGL", 0.0);         /* start height AGL (0 = on ground) */
    double hdg        = envd("FB_HDG", 0.0);               /* runway QFU */
    double spd        = envd("FB_SPEED", 0.0);
    int    use_imu    = envi("FB_HITL_IMU", 0);
    int    fbw_over   = envi("FB_FBW_OVERRIDE", 0);

    fdm=new FGFDMExec(); fdm->SetDebugLevel(0);
    std::string root=models, dir=root+"/"+ac;
    if(!fdm->LoadModel(SGPath(root), SGPath(dir+"/engine"), SGPath(dir+"/Systems"), ac)){
        fprintf(stderr,"[msp_bridge] LoadModel(%s) failed under %s\n",ac,models); return 3; }
    auto ic=fdm->GetIC();
    ic->SetGeodLatitudeDegIC(lat); ic->SetLongitudeDegIC(lon);
    ic->SetAltitudeASLFtIC((gnd+hoff)/FT);
    ic->SetVcalibratedKtsIC(spd*MS2KT);
    ic->SetPsiDegIC(hdg<0?hdg+360:hdg);
    ic->SetFlightPathAngleDegIC(0.0);
    fdm->RunIC();
    { auto pr=fdm->GetPropulsion();                        /* fuel engines need starting; electric self-runs */
      for(unsigned i=0;i<pr->GetNumEngines();i++)
        if(pr->GetEngine(i)->GetType()!=FGEngine::etElectric) pr->InitRunning(i); }
    if(fbw_over) fdm->SetPropertyValue("fcs/fbw-override",1.0);
    fdm->SetPropertyValue("position/terrain-elevation-asl-ft", gnd/FT);
    fdm->Setdt(dt);
    if(spd>0.5){                                          /* airborne start: trim so the IC is a clean flying state */
        try { FGTrim trim(fdm,tLongitudinal); if(!trim.DoTrim()){ fdm->RunIC();
              fprintf(stderr,"[msp_bridge] trim did not converge — clean level IC\n"); } }
        catch(...){ fdm->RunIC(); }
    }

    int fd=msp_connect(host,port);
    if(fd<0){ fprintf(stderr,"[msp_bridge] cannot reach SITL %s:%d\n",host,port); return 4; }
    fprintf(stderr,"[msp_bridge] %s @ %.5f,%.5f gnd=%.0fm hdg=%.0f  HITL->%s:%d imu=%d\n",
            ac,lat,lon,gnd,hdg,host,port,use_imu);

    uint8_t rx[512]; uint16_t rcmd;
    long tick=0;
    for(;;){
        double phi=fdm->GetPropertyValue("attitude/phi-deg");
        double th =fdm->GetPropertyValue("attitude/theta-deg");
        double psi=fdm->GetPropertyValue("attitude/psi-deg"); if(psi<0)psi+=360;
        double glat=fdm->GetPropertyValue("position/lat-geod-deg");
        double glon=fdm->GetPropertyValue("position/long-gc-deg");
        double alt =fdm->GetPropertyValue("position/h-sl-ft")*FT;
        double vN  =fdm->GetPropertyValue("velocities/v-north-fps")*FT;
        double vE  =fdm->GetPropertyValue("velocities/v-east-fps")*FT;
        double vD  =fdm->GetPropertyValue("velocities/v-down-fps")*FT;
        double gs  =fdm->GetPropertyValue("velocities/vg-fps")*FT;
        double p   =fdm->GetPropertyValue("velocities/p-rad_sec")*R2D;
        double q   =fdm->GetPropertyValue("velocities/q-rad_sec")*R2D;
        double r   =fdm->GetPropertyValue("velocities/r-rad_sec")*R2D;
        double crs =atan2(vE,vN)*R2D; if(crs<0)crs+=360;

        /* gravity projected into body frame (g), level = (0,0,1) — truth-attitude iNav
         * ignores this for attitude but its position estimator uses it. */
        double cr=cos(phi*D2R),sr=sin(phi*D2R),ct=cos(th*D2R),st=sin(th*D2R);
        double ax=-st, ay=sr*ct, az=cr*ct;
        double ch=cos(psi*D2R),sh=sin(psi*D2R);            /* level-plane horizontal mag by heading */

        uint8_t tx[128]; int o=0;
        w8(tx,o,2);                                        /* SIMULATOR_MSP_VERSION_2 */
        w8(tx,o,HITL_ENABLE|HITL_HAS_NEW_GPS_DATA|(use_imu?HITL_USE_IMU:0));
        w8(tx,o,2); w8(tx,o,16);                           /* fix 3D, 16 sats */
        w32(tx,o,lround(glat*1e7)); w32(tx,o,lround(glon*1e7)); w32(tx,o,lround(alt*100));
        w16(tx,o,lround(gs*100)); w16(tx,o,lround(crs*10));
        w16(tx,o,lround(vN*100)); w16(tx,o,lround(vE*100)); w16(tx,o,lround(vD*100));
        w16(tx,o,lround(phi*10)); w16(tx,o,lround(th*10)); w16(tx,o,lround(psi*10));
        w16(tx,o,lround(ax*1000)); w16(tx,o,lround(ay*1000)); w16(tx,o,lround(az*1000));
        w16(tx,o,lround(p*16)); w16(tx,o,lround(q*16)); w16(tx,o,lround(r*16));
        w32(tx,o,lround(101325.0*pow(1-2.25577e-5*alt,5.25588)));   /* baro Pa */
        w16(tx,o,lround(ch*16000)); w16(tx,o,lround(-sh*16000)); w16(tx,o,0);

        if(tick<3||tick%50==0) fprintf(stderr,"[dbg] tick=%ld sending %d-byte HITL frame...\n",tick,o);
        if(msp_send(fd,MSP_SIMULATOR,tx,o)<0){ fprintf(stderr,"[msp_bridge] MSP send failed\n"); return 5; }
        int n=msp_recv(fd,&rcmd,rx,sizeof rx);
        if(tick<3) fprintf(stderr,"[dbg] tick=%ld got reply cmd=0x%x n=%d\n",tick,rcmd,n);
        if(n<8){ fprintf(stderr,"[msp_bridge] short HITL reply n=%d\n",n); return 6; }
        int16_t rr=rx[0]|rx[1]<<8, rp=rx[2]|rx[3]<<8, ry=rx[4]|rx[5]<<8, rt=rx[6]|rx[7]<<8;

        double aileron=rr/500.0, elevator=rp/500.0, rudder=ry/500.0;
        double thr=(rt+500)/1000.0; if(thr<0)thr=0; if(thr>1)thr=1;
        double slew=dt/ESC_SPINUP_S;                       /* an ESC ramps; a step blows a light prop's RPM ODE */
        if      (thr>thr_applied+slew) thr_applied+=slew;
        else if (thr<thr_applied-slew) thr_applied-=slew;
        else                           thr_applied =thr;
        fdm->SetPropertyValue("fcs/aileron-cmd-norm",  aileron);
        fdm->SetPropertyValue("fcs/elevator-cmd-norm", -elevator);   /* JSBSim +elev = nose DOWN */
        fdm->SetPropertyValue("fcs/rudder-cmd-norm",   rudder);
        fdm->SetPropertyValue("fcs/throttle-cmd-norm", thr_applied);
        fdm->Run();

        if(++tick % (long)lround(1.0/dt) == 0)
            fprintf(stderr,"[msp_bridge] t=%lds agl=%.0f alt=%.0f gs=%.1f att(r%.0f p%.0f y%.0f) thr=%.2f srv(a%+d e%+d r%+d)\n",
                    tick/(long)lround(1.0/dt), alt-gnd, alt, gs, phi, th, psi, thr_applied, rr, rp, ry);

        struct timespec ts={0,(long)(dt*1e9)}; nanosleep(&ts,NULL);  /* shim scales this for accel */
    }
}
