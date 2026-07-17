/* FlightBox — downlink to flightbox: telemetry + artificial-horizon video. See telemetry.h. */
#include <math.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "telemetry.h"
#include "msp.h"
#include "protocol.h"
#include "../sim_state.h"
#include "../fdm/weather.h"

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

void telem_send(int fbfd, struct sockaddr_in fbdst, int link_up){
    double hd=hypot((S.lat-HOME_LAT)*111320.0,(S.lon-HOME_LON)*111320.0*cos(HOME_LAT*RAD));
    double tohome=atan2f(-(S.lon-HOME_LON),-(S.lat-HOME_LAT))*DEG;
    telem_packet_t t={0}; t.magic=FB_MAGIC_TELEM;
    /* Attitude straight from the FDM (100 Hz) — NOT from iNav's MSP_ATTITUDE, which
     * the bridge only polls at ~5 Hz. That 5 Hz was the "stepped rotation" jitter:
     * position was smooth (100 Hz) but the camera rotated in 5 Hz steps. This is the
     * true airframe attitude anyway (what iNav reads via the X-Plane DREFs). */
    t.roll=(float)S.roll; t.pitch=(float)S.pitch; t.yaw=(float)S.yaw;
    /* x = TRUE east metres (incl. cos(lat)) so home_dist == hypot(x,y) and the renderer's
     * lon reconstruction (divides by 111320*cos) is exact. Was missing the cos factor. */
    t.alt=(float)S.agl; t.x=(float)((S.lon-HOME_LON)*111320.0*cos(HOME_LAT*RAD)); t.y=(float)((S.lat-HOME_LAT)*111320.0);
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
    /* Sequence number: lets the receiver detect LOST packets. Without it a gap in the
     * stream is indistinguishable from a violent manoeuvre — two packets that are
     * really 260 ms apart look like one 10 ms tick, so a normal 2 deg/s roll reads as
     * 148 deg/s. Every consumer must space samples by the seq delta, not by arrival. */
    static uint16_t tseq=0; t.seq=tseq++;
    sendto(fbfd,&t,sizeof t,0,(struct sockaddr*)&fbdst,sizeof fbdst);
}

void telem_video(int fbfd, struct sockaddr_in fbdst){
    static video_packet_t v; render_horizon(&v,t_roll,t_pitch); sendto(fbfd,&v,sizeof v,0,(struct sockaddr*)&fbdst,sizeof fbdst);
}
