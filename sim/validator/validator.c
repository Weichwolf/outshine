/* FlightBox mission validator — a FAST pre-filter before the authoritative flightbox flight.
 *
 * Real libJSBSim physics (the SAME FDM flightbox flies) driven by an iNav-STYLE fixed-wing WP-nav loop,
 * stepped in-process at MAX CPU speed: no clock gate, no MSP, no iNav SITL, no container. It reads a
 * resolved mission (compact text on stdin from the TS resolver), builds the directed ring/sphere gate
 * track from iNav's nav math, flies it, and scores gates + envelope -> OK / FAIL. Orders of magnitude
 * faster than the real stack, so missions iterate quickly; flightbox (real iNav SITL) stays the REFERENCE
 * the nav driver is aligned to — only a validator-OK mission is worth a real flight.
 *
 * Build: g++ validator.c ../aircraft/fdm/jsbsim_adapter.cpp -I../aircraft/fdm <libJSBSim>.a  (see Makefile).
 *
 * Fidelity note: the OUTER nav loop is iNav's own (verbatim cascade). The INNER attitude tracker is a
 * damped, gain-scheduled stand-in for iNav ANGLE mode + the airframe's own inner loop — faithful enough
 * for conventional airframes (c172, SGS validate 3/3), but NOT for a relaxed-stability F-16, whose real
 * FLCS (iNav's rate PID driving fcs/*-cmd-norm, tuned per inav.diff) is what keeps it from departing when
 * it must climb AND turn to a high waypoint. The validator flies the F-16's climb + first capture, then
 * fails-fast on the departure. Validating the F-16 needs iNav's actual ANGLE-mode rate PID here, or the
 * real iNav SITL in the loop; until then the F-16 is validated by flightbox directly (the reference).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jsbsim_adapter.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define G 9.80665
#define D2R (M_PI/180.0)
#define R2D (180.0/M_PI)
#define MAXWP 32
#define MAXGATE 512
#define MAXTRAJ 200000

/* ---------- geometry (local ENU about a reference lat) ---------- */
static double clatf(double lat){ return cos(lat*D2R); }
static void enu(double lat,double lon,double clat0,double clon0,double*e,double*n){
    *e=(lon-clon0)*111320.0*clatf(clat0); *n=(lat-clat0)*111320.0;
}
static void offll(double clat0,double clon0,double e,double n,double*lat,double*lon){
    *lat=clat0+n/111320.0; *lon=clon0+e/(111320.0*clatf(clat0));
}
static double bearing(double a,double b,double c,double d){
    double y=sin((d-b)*D2R)*cos(c*D2R);
    double x=cos(a*D2R)*sin(c*D2R)-sin(a*D2R)*cos(c*D2R)*cos((d-b)*D2R);
    double r=atan2(y,x)*R2D; return r<0?r+360:r;
}
static void dest(double lat,double lon,double brg,double dist,double*ola,double*olo){
    double br=brg*D2R; offll(lat,lon,sin(br)*dist,cos(br)*dist,ola,olo);
}
static double distll(double la0,double lo0,double la1,double lo1){
    double e,n; enu(la1,lo1,la0,lo0,&e,&n); return hypot(e,n);
}
static double wrap180(double x){ while(x>180)x-=360; while(x<-180)x+=360; return x; }

/* ---------- mission (resolved) ---------- */
typedef struct { double lat,lon,elev,hdg; } RW;
typedef struct { double lat,lon,alt_rel; } WP;
typedef struct {
    char ac[32];
    RW to, ld;
    double climb,dive,bank,cruise,approach_len,glide;   /* iNav nav params */
    double cruise_thr,min_thr,max_thr,pitch2thr;        /* iNav nav.fw throttle model (us) */
    double posZp,posZi,posZd,posZff,alt_response,max_climb_rate;  /* fw_alt PID (raw) + alt->rate cascade */
    double vs,vc,vne,vmin;                               /* speed envelope (vmin = min safe maneuvering) */
    double capture_r,climb_alt;                          /* mission: WP fangradius; climb-straight-to alt (AGL) */
    double kpR,kdR,kpP,kdP;                              /* inner attitude-tracker gains (per-aircraft) */
    WP wp[MAXWP]; int nwp;
} Mission;

/* ---------- iNav navPidApply3, copied verbatim from inav-src/common/fp_pid.c (flags=0, measurement-tracking
   D, back-calc anti-windup) so the validator's altitude loop IS iNav's ---------- */
typedef struct { double kP,kI,kD,kFF,kT, integrator, last_input, dTermLpf; int reset, has_last; double dfilt; } NavPid;
static void navpid_init(NavPid*p,double kP,double kI,double kD,double kFF,double dLpf){
    memset(p,0,sizeof*p); p->kP=kP;p->kI=kI;p->kD=kD;p->kFF=kFF;p->dTermLpf=dLpf; p->reset=1;
    if(kI>1e-6&&kP>1e-6){ double Ti=kP/kI,Td=kD/kP; p->kT=2.0/(Ti+Td); }
}
static double navpid(NavPid*p,double setpoint,double meas,double dt,double outMin,double outMax){
    double error=setpoint-meas;
    double P=error*p->kP;
    if(p->reset){ p->last_input=meas; p->reset=0; }
    double deriv=-(meas-p->last_input)/dt; p->last_input=meas;
    if(p->dTermLpf>0){ double rc=1.0/(2*M_PI*p->dTermLpf), a=dt/(rc+dt); p->dfilt+=a*(deriv-p->dfilt); deriv=p->dfilt; }
    double D=p->kD*deriv, FF=setpoint*p->kFF;
    double out=P+p->integrator+D+FF;
    double outC=out<outMin?outMin:out>outMax?outMax:out;
    double back=outC-out; if((back>0)==(p->integrator>0)) back=0;   /* shrink-only anti-windup */
    p->integrator += error*p->kI*dt + back*p->kT*dt;
    return outC;
}

/* ---------- gates ---------- */
typedef struct { double lat,lon,asl,r,brg; int ring; int kind; } Gate;  /* ring=1 directed, 0 sphere; kind 0 climb 1 route 2 approach */
static Gate gate[MAXGATE]; static int ngate;

static void add_ring(double lat,double lon,double asl,double brg,double r,int kind){
    if(ngate>=MAXGATE)return; gate[ngate++]=(Gate){lat,lon,asl,r,brg,1,kind};
}
static double turn_radius(double V,double bank){ return V*V/(G*tan(fmax(5.0,bank)*D2R)); }

/* iNav-math directed corridor: climb-out + straight enroute legs + approach glideslope (mirrors ringtrack). */
static void build_track(const Mission*m,double ring_r,int n_climb,int n_approach){
    ngate=0;
    double spacing=fmax(120.0,turn_radius(m->cruise,m->bank)*0.9);
    double to_elev=m->to.elev, tanC=tan(m->climb*D2R);
    double wp1asl=to_elev+(m->nwp?m->wp[0].alt_rel:100);
    double climb_len=fmax(spacing*n_climb,(wp1asl-to_elev)/fmax(tanC,0.02));
    for(int i=1;i<=n_climb;i++){ double d=climb_len*i/n_climb,la,lo; dest(m->to.lat,m->to.lon,m->to.hdg,d,&la,&lo);
        add_ring(la,lo,to_elev+d*tanC,m->to.hdg,ring_r,0); }
    /* enroute route points: climb-end -> WPs -> approach-start */
    double app_alt=m->ld.elev+m->approach_len*tan(m->glide*D2R);
    double app_la,app_lo; dest(m->ld.lat,m->ld.lon,fmod(m->ld.hdg+180,360),m->approach_len,&app_la,&app_lo);
    double rla[MAXWP+2],rlo[MAXWP+2],rz[MAXWP+2]; int nr=0;
    rla[nr]=gate[ngate-1].lat; rlo[nr]=gate[ngate-1].lon; rz[nr]=gate[ngate-1].asl; nr++;
    for(int i=0;i<m->nwp;i++){ rla[nr]=m->wp[i].lat; rlo[nr]=m->wp[i].lon; rz[nr]=to_elev+m->wp[i].alt_rel; nr++; }
    rla[nr]=app_la; rlo[nr]=app_lo; rz[nr]=app_alt; nr++;
    double length=0; for(int i=0;i<nr-1;i++) length+=distll(rla[i],rlo[i],rla[i+1],rlo[i+1]);
    int need_route=(100-n_climb-n_approach)+nr; if(need_route<1)need_route=1;
    double rspacing=fmin(spacing,length/need_route);
    for(int i=0;i<nr-1;i++){ double seg=distll(rla[i],rlo[i],rla[i+1],rlo[i+1]);
        double br=bearing(rla[i],rlo[i],rla[i+1],rlo[i+1]); int k=(int)(seg/rspacing); if(k<1)k=1;
        for(int j=0;j<k;j++){ double f=(double)j/k;
            add_ring(rla[i]+(rla[i+1]-rla[i])*f,rlo[i]+(rlo[i+1]-rlo[i])*f,rz[i]+(rz[i+1]-rz[i])*f,br,ring_r,1); } }
    double tanG=tan(m->glide*D2R);
    for(int i=n_approach;i>=1;i--){ double d=m->approach_len*i/n_approach,la,lo;
        dest(m->ld.lat,m->ld.lon,fmod(m->ld.hdg+180,360),d,&la,&lo);
        add_ring(la,lo,m->ld.elev+d*tanG,m->ld.hdg,ring_r,2); }
}

/* ---------- gate scoring over a trajectory ---------- */
typedef struct { double lat,lon,asl; } Pt;
static Pt traj[MAXTRAJ]; static int ntraj;

static int pierce_index(const Gate*g,int start){
    double clat=g->lat,clon=g->lon,casl=g->asl,r=g->r;
    if(!g->ring){
        double pe=0,pn=0,pu=0; int have=0;
        for(int i=start;i<ntraj;i++){ double e,n; enu(traj[i].lat,traj[i].lon,clat,clon,&e,&n); double u=traj[i].asl-casl;
            if(hypot(hypot(e,n),u)<r) return i;
            if(have){ double de=e-pe,dn=n-pn,du=u-pu,L2=de*de+dn*dn+du*du;
                if(L2>1e-9){ double t=-(pe*de+pn*dn+pu*du)/L2; if(t<0)t=0; if(t>1)t=1;
                    if(hypot(hypot(pe+t*de,pn+t*dn),pu+t*du)<r) return i; } }
            pe=e;pn=n;pu=u;have=1; }
        return -1;
    }
    double br=g->brg*D2R,ne=sin(br),nn=cos(br); double pe=0,pn=0,pu=0,ps=0; int have=0;
    for(int i=start;i<ntraj;i++){ double e,n; enu(traj[i].lat,traj[i].lon,clat,clon,&e,&n); double u=traj[i].asl-casl,s=e*ne+n*nn;
        if(have&&ps<0&&s>=0){ double t=ps/(ps-s);
            double ct=(pe+t*(e-pe))*(-nn)+(pn+t*(n-pn))*ne, cu=pu+t*(u-pu);
            if(hypot(ct,cu)<r) return i; }
        pe=e;pn=n;pu=u;ps=s;have=1; }
    return -1;
}
static void score(int*threaded,int*inorder){
    *threaded=0; for(int i=0;i<ngate;i++) if(pierce_index(&gate[i],0)>=0)(*threaded)++;
    int io=0,cur=0,idx=0;
    for(int i=0;i<ngate;i++){ int j=pierce_index(&gate[i],idx); if(j>=0){cur++;idx=j;if(cur>io)io=cur;} else cur=0; }
    *inorder=io;
}

/* ---------- input parse (compact, from the TS resolver) ---------- */
static int parse(Mission*m){
    memset(m,0,sizeof*m); char line[256];
    while(fgets(line,sizeof line,stdin)){
        if(!strncmp(line,"ac ",3)) sscanf(line+3,"%31s",m->ac);
        else if(!strncmp(line,"to ",3)) sscanf(line+3,"%lf %lf %lf %lf",&m->to.lat,&m->to.lon,&m->to.elev,&m->to.hdg);
        else if(!strncmp(line,"ld ",3)) sscanf(line+3,"%lf %lf %lf %lf",&m->ld.lat,&m->ld.lon,&m->ld.elev,&m->ld.hdg);
        else if(!strncmp(line,"nav ",4)) sscanf(line+4,"%lf %lf %lf %lf %lf %lf",&m->climb,&m->dive,&m->bank,&m->cruise,&m->approach_len,&m->glide);
        else if(!strncmp(line,"thr ",4)) sscanf(line+4,"%lf %lf %lf %lf",&m->cruise_thr,&m->min_thr,&m->max_thr,&m->pitch2thr);
        else if(!strncmp(line,"pid ",4)) sscanf(line+4,"%lf %lf %lf %lf %lf %lf",&m->posZp,&m->posZi,&m->posZd,&m->posZff,&m->alt_response,&m->max_climb_rate);
        else if(!strncmp(line,"spd ",4)){ m->vmin=0; sscanf(line+4,"%lf %lf %lf %lf",&m->vs,&m->vc,&m->vne,&m->vmin); if(m->vmin<=0)m->vmin=1.2*m->vs; }
        else if(!strncmp(line,"cfg ",4)) sscanf(line+4,"%lf %lf",&m->capture_r,&m->climb_alt);
        else if(!strncmp(line,"gain ",5)) sscanf(line+5,"%lf %lf %lf %lf",&m->kpR,&m->kdR,&m->kpP,&m->kdP);
        else if(!strncmp(line,"wp ",3)&&m->nwp<MAXWP){ sscanf(line+3,"%lf %lf %lf",&m->wp[m->nwp].lat,&m->wp[m->nwp].lon,&m->wp[m->nwp].alt_rel); m->nwp++; }
    }
    return m->ac[0]!=0 && m->nwp>0;
}

int main(void){
    Mission m;
    if(!parse(&m)){ fprintf(stderr,"validator: bad/empty mission on stdin\n"); return 2; }
    build_track(&m,150.0,12,12);

    const char*mr=getenv("MODELS_ROOT"); if(!mr||!*mr) mr="models";
    /* spawn on the runway, aligned, at cruise; iNav-style nav will fly it. hoff<0 = sit on the gear. */
    if(fb_jsbsim_init(mr,m.ac,m.to.lat,m.to.lon,m.to.elev,-1.0,m.cruise,m.to.hdg,0)!=0){
        fprintf(stderr,"validator: JSBSim init failed for %s\n",m.ac); return 2;
    }
    double clr=fb_jsbsim_ground_clearance(1);
    /* -------- fly: iNav-style WP nav (outer) + damped attitude hold (inner) driving the real FDM, max
       CPU speed. set_controls sets SURFACE deflections, so a plain P on heading diverges — the inner
       loop tracks a bank/pitch COMMAND with body-rate damping (p,q). Gains are aligned to flightbox. --- */
    /* inner attitude tracker = the ANGLE-mode stand-in: nav emits target roll/pitch ANGLES, this drives the
       surfaces toward them with body-rate damping (p,q). iNav's own nav cascade below produces the angles. */
    const double KP_ROLL=m.kpR>0?m.kpR:0.030, KD_ROLL=m.kdR>0?m.kdR:0.010;
    const double KP_PITCH=m.kpP>0?m.kpP:0.055, KD_PITCH=m.kdP>0?m.kdP:0.018;
    NavPid pidAlt; navpid_init(&pidAlt, m.posZp/100.0, m.posZi/100.0, m.posZd/300.0, m.posZff/100.0, 10.0);  /* fw_alt (climb-rate branch) */
    double vtarget=fmax(m.cruise, m.vmin*1.2);       /* operating airspeed: sustainable at cruise power, above departure */
    int wp=0; int landing=0; int launched=0; int landed=0; int landphase=0; double t=0; const double dt=0.01, t_max=1200.0;
    double wp1rel = m.nwp? m.wp[0].alt_rel : 100;
    double wpmin[MAXWP]; for(int i=0;i<m.nwp;i++) wpmin[i]=1e9;
    double tdist=1e9;                                  /* closest horizontal approach to the landing threshold */
    char fail[128]=""; int crashed=0;
    fb_fdm_state st; memset(&st,0,sizeof st);
    st.lat=m.to.lat; st.lon=m.to.lon; st.elev=m.to.elev+clr; st.yaw=m.to.hdg; st.speed=m.cruise;   /* IC seed */
    while(t<t_max){
        double agl=st.elev-m.to.elev;
        /* --- outer nav: LAUNCH (straight climb-out on runway heading, matches the climb corridor), then
           WP nav, then approach + threshold. iNav flies a wings-level launch climb before engaging WPs. --- */
        double tgt_la,tgt_lo,tgt_alt,tgt_spd; double desHdg=-999.0;   /* desHdg set -> localizer heading, else home to target */
        if(!launched){
            /* wings-level climb-out straight on the runway heading to the climb altitude, THEN WP nav. A fast
               relaxed-stability jet cannot climb AND turn (energy bleed -> departure), so it climbs straight
               to angle_hold_alt first (iNav "full-ANGLE climb"); light aircraft just need a brief climb-out. */
            dest(m.to.lat,m.to.lon,m.to.hdg,20000.0,&tgt_la,&tgt_lo);
            double climbTo = m.climb_alt>0 ? m.climb_alt : fmin(wp1rel*0.9,100.0);
            tgt_alt=m.to.elev+climbTo; tgt_spd=m.cruise;
            if(agl>=climbTo*0.95) launched=1;
        } else if(wp<m.nwp){ tgt_la=m.wp[wp].lat; tgt_lo=m.wp[wp].lon; tgt_alt=m.to.elev+m.wp[wp].alt_rel; tgt_spd=m.cruise; }
        else {
            /* iNav-autoland-style: capture an approach fix (approach_len behind the threshold) with a radius
               wider than the turn circle so it always establishes, then fly a LOCALIZER final — hold the
               landing heading, correct cross-track to the extended centreline, descend the glideslope by
               ALONG-track distance. A straight, aligned final crosses the threshold at ground level; homing
               to the threshold point just orbits it (turn radius > touchdown radius) and never lands. */
            landing=1; tgt_spd=m.vs*1.3;
            double Rt=m.cruise*m.cruise/(G*tan(fmax(5.0,m.bank)*D2R)), capR=fmax(200.0,2.2*Rt);
            double final_len=fmax(m.approach_len,700.0);              /* long final -> room to null cross-track */
            double apla,aplo; dest(m.ld.lat,m.ld.lon,fmod(m.ld.hdg+180.0,360.0),final_len,&apla,&aplo);
            if(landphase==0){
                tgt_la=apla; tgt_lo=aplo; tgt_alt=m.ld.elev+clr+final_len*tan(m.glide*D2R);
                if(distll(st.lat,st.lon,apla,aplo)<capR) landphase=1;
            } else {
                /* carrot / L1 pursuit down the extended centreline: aim at a point a lookahead ahead of the
                   aircraft's projection onto the line -> smoothly intercepts and tracks it (a linear crab
                   oscillates). Glideslope by along-track distance to the threshold. */
                double hr=m.ld.hdg*D2R, fe=sin(hr), fn=cos(hr);      /* forward = landing direction (E,N) */
                double e,n; enu(st.lat,st.lon,m.ld.lat,m.ld.lon,&e,&n);
                double along=e*fe+n*fn;                              /* <0 approach side, >0 past threshold */
                double s_aim=along+300.0;                            /* carrot 300 m ahead toward the threshold */
                offll(m.ld.lat,m.ld.lon,s_aim*fe,s_aim*fn,&tgt_la,&tgt_lo);
                tgt_alt=m.ld.elev+clr+fmax(0.0,-along)*tan(m.glide*D2R);
            }
        }
        double desH = desHdg>-900.0? desHdg : bearing(st.lat,st.lon,tgt_la,tgt_lo);
        double herr=wrap180(desH-st.yaw);
        /* gentle-dogleg: cap bank below the nav limit when energy-limited — a hard bank at low speed margin
           bleeds a relaxed-stability jet into a departure. Full nav bank once comfortably above vmin. */
        double smarg=(st.speed-m.vmin)/fmax(1.0,vtarget*1.3-m.vmin);   /* full bank only well above operating speed */
        double bankMax=m.bank*fmax(0.25,fmin(1.0,smarg));              /* gentle turns preserve a jet's energy */
        double bank_cmd=fmax(-bankMax,fmin(bankMax, herr*1.5));         /* course->bank, energy-capped */
        (void)tgt_spd;
        /* --- iNav fixed-wing altitude cascade (navigation_fixedwing.c): altitude error -> desired climb rate
           (alt_control_response, capped at max_auto_climb_rate), then the fw_alt PID on climb-rate error ->
           target pitch angle, clamped to [-dive, climb]. This is iNav's own law, not a stand-in. --- */
        double altErr_cm=(tgt_alt-st.elev)*100.0;
        double pitch_cmd;
        if(!launched){
            /* full-ANGLE climb (the flightbox F-16 recipe): during the initial climb-out hold the climb
               ANGLE directly, bypassing iNav's nav auto-climb-RATE limit — a jet climbs faster than that
               cap, and the rate PID would otherwise command nose-down and upset a relaxed-stability airframe. */
            pitch_cmd=m.climb;
        } else {
            /* iNav fixed-wing altitude cascade (navigation_fixedwing.c): altitude error -> desired climb rate
               (alt_control_response, capped at max_auto_climb_rate), then the fw_alt PID on climb-rate error
               -> target pitch angle, clamped to [-dive, climb]. This is iNav's own law. */
            double desClimb=m.alt_response*altErr_cm/100.0;             /* cm/s */
            desClimb=fmax(-m.max_climb_rate,fmin(m.max_climb_rate,desClimb));
            double actClimb=st.vy*100.0;                               /* cm/s, +up */
            double pitch_dd=navpid(&pidAlt,desClimb,actClimb,dt,-m.dive*10.0,m.climb*10.0);
            pitch_cmd=pitch_dd/10.0;                                    /* deg */
        }
        /* energy management: never demand more climb than airspeed allows — below the min maneuvering speed
           trade climb for speed (nose down) so a low-excess-thrust / relaxed-stability airframe never bleeds
           into a departure. */
        if(st.speed<m.vmin && agl>30.0) pitch_cmd=fmin(pitch_cmd,(st.speed-m.vmin)*0.5);
        /* --- iNav throttle: cruise + pitch*pitch2thr (us). SITL iNav has no airspeed (min-speed uses GPS
           groundspeed @ 7 m/s), so it won't hold speed — a low-excess-thrust airframe (the F-16 at nav
           cruise) mushes off the back of the power curve in climbs/turns. Flightbox flies it with a MANUAL
           throttle hand (the "full-ANGLE climb" recipe); we model that: push throttle up toward max when
           airspeed falls below the nav reference. Near/above cruise this is ~0 (light aircraft unaffected). */
        /* throttle = iNav cruise + pitch2thr, plus a SPEED-HOLD toward a target airspeed. SITL iNav can't
           hold speed; flightbox flies the jet with a throttle hand that keeps it in its controllable band
           (above departure, below the high-q PIO regime). vtarget sits at the airframe's real operating
           speed (nav-reference cruise, or above the departure margin for a relaxed-stability jet). */
        double sperr=vtarget-st.speed;
        double thr_us=m.cruise_thr + pitch_cmd*m.pitch2thr + (sperr>0? sperr*40.0 : sperr*12.0);  /* boost hard slow, cut gently fast */
        if(altErr_cm>3000.0) thr_us=fmax(thr_us, m.min_thr+0.7*(m.max_thr-m.min_thr));  /* sustained climb -> hold power up */
        if(agl>30.0 && (pitch_cmd>-3.0||fabs(bank_cmd)>4.0) && st.speed<m.vne*0.85) thr_us=fmax(thr_us,m.cruise_thr+0.4*(m.max_thr-m.cruise_thr)); /* airborne level/climb OR turning: hold power (a jet decays fast at low power) */
        if(st.speed>vtarget*1.25) thr_us=fmin(thr_us,m.cruise_thr);    /* keep within the operating band */
        if(st.speed>m.vne*0.9) thr_us=fmin(thr_us,m.min_thr);          /* vne protection */
        thr_us=fmax(m.min_thr,fmin(m.max_thr,thr_us));
        double thr=(thr_us-1000.0)/1000.0;
        /* --- inner: damped surface commands toward the nav-commanded angles, gain-scheduled by dynamic
           pressure (~1/V²). Fixed gains over a jet's wide speed band over-control at high q (PIO/balloon)
           and under-control slow; scaling by (cruise/V)² keeps the response consistent, as an FLCS does. */
        double qs=(vtarget*vtarget)/(st.speed*st.speed+1.0); qs=fmax(0.15,fmin(2.0,qs));
        double roll = fmax(-1.0,fmin(1.0, qs*KP_ROLL*(bank_cmd-st.roll) - KD_ROLL*st.p));
        double pitch= fmax(-1.0,fmin(1.0, qs*KP_PITCH*(pitch_cmd-st.pitch) - KD_PITCH*st.q));
        /* FDM ground under the aircraft: the landing field once on approach, else takeoff. Takeoff and
           landing airports differ in elevation -> AGL for crash/stall/touchdown must be vs the LOCAL ground,
           while launch/WP altitudes stay home-relative (takeoff). */
        double ground = landing ? m.ld.elev : m.to.elev;
        fb_jsbsim_set_ground(ground);
        fb_jsbsim_set_controls(roll,pitch,0.0,thr);
        fb_jsbsim_step(&st);
        if(getenv("VTRACE") && ((int)(t/dt)%500==0))
            fprintf(stderr,"t=%5.1f agl=%6.1f spd=%5.1f vz=%+5.1f roll=%+5.1f pitch=%+5.1f | bank_cmd=%+5.1f pit_cmd=%+5.1f thr=%.2f ph=%s%d wp=%d dth=%.0f tgtalt=%.0f\n",
                t,agl,st.speed,st.vy,st.roll,st.pitch,bank_cmd,pitch_cmd,thr,!launched?"LNCH":landing?"LAND":"WP",landphase,wp,distll(st.lat,st.lon,m.ld.lat,m.ld.lon),tgt_alt-m.ld.elev);
        if(ntraj<MAXTRAJ){ traj[ntraj].lat=st.lat; traj[ntraj].lon=st.lon; traj[ntraj].asl=st.elev; ntraj++; }
        /* --- envelope guards (fail-fast) --- */
        if(!(isfinite(st.lat)&&isfinite(st.lon)&&isfinite(st.elev))){ snprintf(fail,sizeof fail,"NaN/divergence at t=%.1f",t); crashed=1; break; }
        agl=st.elev-m.to.elev;                                          /* home-relative (launch/WP datum) */
        double aglL=st.elev-ground;                                     /* local AGL (crash/stall/touchdown) */
        if(aglL < -1.0){ snprintf(fail,sizeof fail,"below ground (crash) at t=%.1f",t); crashed=1; break; }
        if(!landing && launched && aglL < clr+1.5 && t>8.0){ snprintf(fail,sizeof fail,"ground contact before landing (departure/crash) at t=%.1f",t); crashed=1; break; }
        if(st.speed < m.vs*0.9 && aglL>5){ snprintf(fail,sizeof fail,"stalled: TAS %.1f < %.1f at t=%.1f",st.speed,m.vs*0.9,t); crashed=1; break; }
        if(st.speed > m.vne){ snprintf(fail,sizeof fail,"overspeed: TAS %.1f > vne %.1f at t=%.1f",st.speed,m.vne,t); crashed=1; break; }
        /* --- WP capture / touchdown (the flightbox orakel: reach each WP in the capture radius, land near
           the threshold; envelope must hold throughout) --- */
        double capR = m.capture_r>0? m.capture_r : 150.0;
        if(launched && wp<m.nwp){ double dw=distll(st.lat,st.lon,tgt_la,tgt_lo); if(dw<wpmin[wp])wpmin[wp]=dw;
            if(dw<capR) wp++; }
        if(landing){ double dl=distll(st.lat,st.lon,m.ld.lat,m.ld.lon); if(dl<tdist)tdist=dl;
            if(dl<150.0 && aglL<clr+5){ landed=1; break; } }
        t+=dt;
    }
    int threaded,inorder; score(&threaded,&inorder);
    if(getenv("VDEBUG")){
        int idx=0;
        for(int i=0;i<ngate;i++){ int j=pierce_index(&gate[i],idx);
            double best=1e9,bh=0,bv=0; for(int k=0;k<ntraj;k++){ double e,n; enu(traj[k].lat,traj[k].lon,gate[i].lat,gate[i].lon,&e,&n);
                double u=traj[k].asl-gate[i].asl,d=sqrt(e*e+n*n+u*u); if(d<best){best=d;bh=hypot(e,n);bv=u;} }
            fprintf(stderr,"gate %3d %-5s brg=%3.0f asl=%5.0f  pierce@%-6d closest=%5.0fm (h=%.0f v=%+.0f)\n",
                i, gate[i].kind==0?"climb":gate[i].kind==1?"route":"appr", gate[i].brg, gate[i].asl, j, best, bh, bv);
            if(j>=0)idx=j; }
        fprintf(stderr,"--- traj n=%d, final pos %.5f,%.5f asl=%.0f ---\n",ntraj, ntraj?traj[ntraj-1].lat:0, ntraj?traj[ntraj-1].lon:0, ntraj?traj[ntraj-1].asl:0);
    }
    /* verdict: flightbox orakel — launched, every WP captured, touched down near the threshold, envelope
       never violated. The ring in-order run is a reported route-fidelity diagnostic, not the gate. */
    double wpworst=0; for(int i=0;i<m.nwp;i++) if(wpmin[i]>wpworst) wpworst=wpmin[i];
    if(!crashed){
        if(!launched) snprintf(fail,sizeof fail,"never launched");
        else if(wp<m.nwp) snprintf(fail,sizeof fail,"WP%d missed (closest %.0fm)",wp,wpmin[wp]);
        else if(!landed) snprintf(fail,sizeof fail,"no touchdown (closest to threshold %.0fm)",tdist);
    }
    int ok = !crashed && launched && wp>=m.nwp && landed;
    printf("%s ac=%s wp=%d/%d worst-capture=%.0fm touchdown=%.0fm rings=%d/%d(io) t=%.0fs%s%s\n",
           ok?"OK":"FAIL", m.ac, wp, m.nwp, wpworst, landed?tdist:-1, inorder, ngate, t,
           fail[0]?"  ":"", fail);
    return ok?0:1;
}
