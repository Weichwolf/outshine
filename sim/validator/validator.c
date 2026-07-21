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
 * Fidelity — precise about what is iNav's code vs a replication:
 *   - NAV controllers (fw_alt altitude cascade, fw_nav heading->roll) call iNav's OWN navPidApply2/navPidInit,
 *     EXTRACTED verbatim into generated/inav_core.h by extract_inav.mjs (re-run on an inav-src bump).
 *   - The FW rate loop (pidApplyFixedWingRateController) is too firmware-coupled to extract standalone, so
 *     fw_inner() is a documented FAITHFUL REPLICATION of its P·rateErr + FF·rateTarget + I core (with iNav's
 *     extracted scaling multipliers) — not a verbatim copy. iNav's low-P/high-FF gains stabilise a jet.
 *   - Aircraft params come only from the model files flightbox flies: all {model}*.xml (physics, via
 *     libJSBSim) + inav.diff (control, every setting accounted — see ringtrack.ts assertInavCoverage).
 * SCOPE: this validator is a pre-filter for CONVENTIONAL airframes (c172, SGS 2-33) — they validate WP capture
 * + a full aligned touchdown-and-rollout worldwide. The FAST-JET F-16 is NOT validated here and is validated by
 * FLIGHTBOX directly (the authoritative reference — it flies the real iNav). The validator's simplified
 * fixed-wing energy/throttle model cannot fly the F-16 enroute: its natural cruise (~90 m/s) is far above the
 * low iNav nav-reference airspeed (fw_reference_airspeed -> vc≈55), so altitude-hold at a WP commands nose-up
 * that the jet's thin thrust margin can't sustain -> it bleeds speed and mushes. Modelling that faithfully needs
 * the jet's drag polar + the manual full-ANGLE-climb throttle hand flightbox flies it with — an honest gap, not
 * a silent one. (The maneuver-speed floor + enroute throttle handling below improve it but don't close the gap.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jsbsim_adapter.h"
#include "generated/inav_core.h"   /* iNav's numeric control core, extracted verbatim (extract_inav.mjs) */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define G 9.80665
#define D2R (M_PI/180.0)
#define R2D (180.0/M_PI)
#define NAV_WP_RADIUS_M 15.0   /* iNav nav_wp_radius fallback: fixed-wing reaches a WP by the bearing-swing passby, so this small radius rarely fires — it just catches a WP overflown dead-centre in one discrete step */
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
typedef struct { double lat,lon,elev,hdg,len; } RW;
typedef struct { double lat,lon,alt_rel; } WP;
typedef struct {
    char ac[32];
    RW to, ld;
    double climb,dive,bank,cruise,approach_len,glide;   /* iNav nav params */
    double cruise_thr,min_thr,max_thr,pitch2thr;        /* iNav nav.fw throttle model (us) */
    double posZp,posZi,posZd,posZff,alt_response,max_climb_rate,control_smoothness;  /* fw_alt PID + alt->rate cascade + pitch-cmd smoothing */
    double posXYp,posXYi,posXYd;                        /* fw_nav (heading->roll) PID, raw */
    double vs,vc,vne,vmin;                               /* speed envelope (vmin = min safe maneuvering) */
    double capture_r,climb_alt;                          /* mission: WP fangradius; climb-straight-to alt (AGL) */
    double kpR,kdR,kpP,kdP;                              /* inner attitude-tracker gains (legacy, unused when rate PID present) */
    double fwPr,fwIr,fwDr,fwFfr,fwPp,fwIp,fwDp,fwFfp,pLevel,iLevel;  /* iNav ANGLE mode + FW rate PID (raw) */
    double fwPy,turnAssist;                              /* yaw rate PID P + turn-assist gain (coordinated turns) */
    double launchClimbAngle,launchTimeout;              /* NAV LAUNCH climb angle (deg) + timeout (ms) */
    double loiterRadius,rthAltitude;                    /* loiter/orbit radius (m); RTH altitude (m) */
    WP wp[MAXWP]; int nwp;
} Mission;

/* iNav fixed-wing inner loop — a faithful REPLICATION of ANGLE mode (pidLevel: angle error -> rate target
   via fw_p_level, LPF'd) + the FW rate PID (pidApplyFixedWingRateController: P·rateErr + FF·rateTarget + I,
   clamped to pidSumLimit). The scaling multipliers (FP_PID_*_MULTIPLIER) and getSmoothnessCutoffFreq come
   from the EXTRACTED inav_core.h; the rate loop itself is too firmware-coupled to extract verbatim. iNav's
   low-P/high-FF gains (inav.diff) are what stabilise a relaxed-stability airframe. */
#define PIDSUM_LIMIT 500.0                   /* fw_pidsum_limit default; roll/pitch servo saturates here */
typedef struct { double iterm, lpf; int lpf_init; } FwAxis;
static double fw_inner(FwAxis*s,double angle_cmd,double attitude,double gyro,double P,double I,double D,double FF,double pLevel,double iLevel,double maxRate,double dt){
    double angleErr=angle_cmd-attitude;
    double rateTarget=angleErr*(pLevel*FP_PID_LEVEL_P_MULTIPLIER);
    if(rateTarget>maxRate)rateTarget=maxRate; else if(rateTarget<-maxRate)rateTarget=-maxRate;
    if(iLevel>0){ double rc=1.0/(2*M_PI*iLevel), a=dt/(rc+dt); if(!s->lpf_init){s->lpf=rateTarget;s->lpf_init=1;} s->lpf+=a*(rateTarget-s->lpf); rateTarget=s->lpf; }
    double kP=P/FP_PID_RATE_P_MULTIPLIER, kI=I/FP_PID_RATE_I_MULTIPLIER, kFF=FF/FP_PID_RATE_FF_MULTIPLIER;
    /* iNav's ITerm-lock ATTENUATION (pid.c iTermLockApply + maths.c attenuation = a Gaussian): when |rateTarget|
       is high — a hard command, the FF-driven regime — P/D/I are damped so they don't FIGHT the feedforward and
       oscillate. σ = (maxRate·fwItermLockRateLimit/100)/2.35482 (default 40%). This is the real iNav mechanism
       the earlier hand-rolled FW_RATE_DAMP crudely (and, the sim-critic showed, inertly) approximated — the
       missing piece that let the low-roll-damping SGS glider self-oscillate in the replication. */
    double sigma=(maxRate*0.40)/2.35482;
    double aPD=exp(-(rateTarget*rateTarget)/(2.0*sigma*sigma));
    double rateErr=rateTarget-gyro;
    double pTerm=rateErr*kP*aPD, ffTerm=rateTarget*kFF;
    s->iterm += rateErr*kI*dt*aPD;
    if(s->iterm>PIDSUM_LIMIT)s->iterm=PIDSUM_LIMIT; else if(s->iterm<-PIDSUM_LIMIT)s->iterm=-PIDSUM_LIMIT;
    double dTerm=-D/1000.0*gyro*aPD;   /* D on gyro (0 for these airframes' fw_d_* gains) */
    double axisPID=pTerm+ffTerm+s->iterm+dTerm;
    if(axisPID>PIDSUM_LIMIT)axisPID=PIDSUM_LIMIT; else if(axisPID<-PIDSUM_LIMIT)axisPID=-PIDSUM_LIMIT;
    return axisPID/PIDSUM_LIMIT;   /* normalized servo (-1..1) */
}

/* The NAV controllers (fw_alt altitude cascade, fw_nav heading) call iNav's OWN navPidApply2/navPidInit
   from the extracted inav_core.h — not a re-implementation. (The FW rate loop, pidApplyFixedWingRateController,
   is too firmware-coupled to extract standalone — navConfig/pidProfile/FLIGHT_MODE/autotune — so fw_inner
   below is a documented faithful replication of its P·rateErr + FF·rateTarget + I core, NOT a verbatim copy.) */

/* ---------- gates ---------- */
typedef struct { double lat,lon,asl,r,brg; int ring; int kind; } Gate;  /* ring=1 directed, 0 sphere; kind 0 climb 1 route 2 approach */
static Gate gate[MAXGATE]; static int ngate;

static void add_ring(double lat,double lon,double asl,double brg,double r,int kind){
    if(ngate>=MAXGATE) return;
    gate[ngate++]=(Gate){lat,lon,asl,r,brg,1,kind};
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
        else if(!strncmp(line,"ld ",3)){ m->ld.len=1500; sscanf(line+3,"%lf %lf %lf %lf %lf",&m->ld.lat,&m->ld.lon,&m->ld.elev,&m->ld.hdg,&m->ld.len); }
        else if(!strncmp(line,"nav ",4)) sscanf(line+4,"%lf %lf %lf %lf %lf %lf",&m->climb,&m->dive,&m->bank,&m->cruise,&m->approach_len,&m->glide);
        else if(!strncmp(line,"thr ",4)) sscanf(line+4,"%lf %lf %lf %lf",&m->cruise_thr,&m->min_thr,&m->max_thr,&m->pitch2thr);
        else if(!strncmp(line,"pid ",4)) sscanf(line+4,"%lf %lf %lf %lf %lf %lf %lf",&m->posZp,&m->posZi,&m->posZd,&m->posZff,&m->alt_response,&m->max_climb_rate,&m->control_smoothness);
        else if(!strncmp(line,"posxy ",6)) sscanf(line+6,"%lf %lf %lf",&m->posXYp,&m->posXYi,&m->posXYd);
        else if(!strncmp(line,"spd ",4)){ m->vmin=0; sscanf(line+4,"%lf %lf %lf %lf",&m->vs,&m->vc,&m->vne,&m->vmin); if(m->vmin<=0)m->vmin=1.2*m->vs; }
        else if(!strncmp(line,"cfg ",4)) sscanf(line+4,"%lf %lf",&m->capture_r,&m->climb_alt);
        else if(!strncmp(line,"gain ",5)) sscanf(line+5,"%lf %lf %lf %lf",&m->kpR,&m->kdR,&m->kpP,&m->kdP);
        else if(!strncmp(line,"rate ",5)) sscanf(line+5,"%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",&m->fwPr,&m->fwIr,&m->fwDr,&m->fwFfr,&m->fwPp,&m->fwIp,&m->fwDp,&m->fwFfp,&m->pLevel,&m->iLevel);
        else if(!strncmp(line,"yaw ",4)) sscanf(line+4,"%lf %lf",&m->fwPy,&m->turnAssist);
        else if(!strncmp(line,"launch ",7)) sscanf(line+7,"%lf %lf",&m->launchClimbAngle,&m->launchTimeout);
        else if(!strncmp(line,"loiter ",7)) sscanf(line+7,"%lf %lf",&m->loiterRadius,&m->rthAltitude);
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
    FwAxis axRoll={0}, axPitch={0};                  /* iNav FW rate-PID state per axis (iterm, angle LPF) */
    /* fw_alt (climb-rate branch) + fw_nav (heading) = iNav's OWN navPidApply2 from inav_core.h. Gains scaled
       exactly as navigation.c inits them: fw_alt = pos_z P/100,I/100,D/300,FF/100; fw_nav = pos_xy P,I,D /100. */
    pidController_t pidAlt; navPidInit(&pidAlt, m.posZp/100.0f, m.posZi/100.0f, m.posZd/300.0f, m.posZff/100.0f, NAV_DTERM_CUT_HZ, 0.0f);
    pidController_t pidNav; navPidInit(&pidNav, m.posXYp/100.0f, m.posXYi/100.0f, m.posXYd/100.0f, 0.0f, NAV_DTERM_CUT_HZ, 0.0f);
    double vtarget=fmax(m.cruise, m.vmin*1.2);       /* operating airspeed: sustainable at cruise power, above departure */
    int jet = m.vmin > 1.5*m.vs;                      /* relaxed-stability / high-min-speed airframe: must hold power always */
    double pcut  = getSmoothnessCutoffFreq(NAV_FW_BASE_PITCH_CUTOFF_FREQUENCY_HZ, (int)m.control_smoothness);  /* iNav pitch-cmd LPF cutoff (Hz), iNav's OWN extracted fn */
    double rcut  = getSmoothnessCutoffFreq(NAV_FW_BASE_ROLL_CUTOFF_FREQUENCY_HZ,  (int)m.control_smoothness);  /* iNav roll-cmd LPF cutoff — DIFFERENT base (roll bandwidth ≫ pitch): c172 cs4 -> 2.26 Hz, not the pitch 0.53 */
    double pitchLpf=0; int pitchLpfInit=0; double rollLpf=0; int rollLpfInit=0;
    int wp=0; int landing=0; int launched=0; int landed=0; int landphase=0; int committed=0; int flaring=0; double t=0; const double dt=0.01, t_max=2400.0;
    double wp1rel = m.nwp? m.wp[0].alt_rel : 100;
    double wpmin[MAXWP]; for(int i=0;i<m.nwp;i++) wpmin[i]=1e9;
    double tdist=1e9;                                  /* closest horizontal approach to the landing threshold */
    /* landing rollout: at touchdown the engine is cut + wheel brakes applied, and the ground roll is
       simulated to a full STOP. td_* capture where/how it touched the runway; rollout is the distance from
       touchdown to standstill; a real landing is ON the runway (aligned, on the strip) AND stops on it. */
    int rolling=0; double td_along=0,td_xte=0,td_hdgerr=0,td_speed=0,td_cas=0,td_vz=0,rollout=0,td_lat=0,td_lon=0;
    int wpBrgSet=-1; double wpInitBrg=0;   /* iNav isWaypointReached: initial bearing to the active WP (bearing-swing detects passing it) */
    double prevHeadErr=0, monTimer=0; int errDecr=0;   /* iNav fw_nav 2 Hz error-monitoring (PID_SHRINK_INTEGRATOR when converging) */
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
            landing=1;
            double final_len=fmax(m.approach_len,2500.0);            /* FAF distance behind the threshold */
            double hr=m.ld.hdg*D2R, fe=sin(hr), fn=cos(hr);          /* runway forward = landing direction (E,N) */
            double e,n; enu(st.lat,st.lon,m.ld.lat,m.ld.lon,&e,&n);
            double along=e*fe+n*fn;                                  /* <0 approach side (behind threshold), >0 past it */
            double pattern_alt=m.ld.elev+clr+final_len*tan(m.glide*D2R);
            /* latched: 0 GET BEHIND the threshold (wrong-side entries only) -> 1 FINAL. No heading-control
               intercept phase — that hits a 180° singularity when the join leaves the aircraft outbound; a
               sliding-carrot pure pursuit (bearing to a point, never 180°-ambiguous) turns final cleanly. */
            if(landphase==0 && along < -400.0) landphase=1;
            if(landphase==0){
                /* GET BEHIND: fly to the FAF (final_len behind the threshold on the centreline). Only runs when
                   the task left the aircraft ahead of/abeam the threshold; exits at along<-400 (before it can
                   orbit the fix). Hold pattern altitude. */
                dest(m.ld.lat,m.ld.lon,fmod(m.ld.hdg+180.0,360.0),final_len,&tgt_la,&tgt_lo);
                tgt_alt=pattern_alt;
            } else {
                /* FINAL: sliding-carrot pure pursuit down the extended centreline toward (and past) the
                   threshold — captures the centreline AND flies the course reversal (the carrot is a point
                   ahead on the line, so the bearing gives a definite turn direction). Descend the glideslope
                   by along-track ONLY once established (on the localizer, aligned) so it doesn't dive mid-turn. */
                double L=fmax(200.0,2.5*turn_radius(m.cruise,m.bank));
                double s_aim=along+L;
                offll(m.ld.lat,m.ld.lon,s_aim*fe,s_aim*fn,&tgt_la,&tgt_lo);
                double gslope=m.ld.elev+clr+fmax(0.0,-along)*tan(m.glide*D2R);
                /* descend the glideslope by along-track once behind the threshold and heading inbound — the
                   lateral pursuit centres the aircraft CONCURRENTLY, so it arrives on-slope AND centred instead
                   of holding pattern altitude until aligned late and then diving (a steep, fast, bouncing
                   touchdown). COMMIT once into short final: keep descending, never revert to pattern altitude
                   (a flicker in `established` at the flare would command a full-throttle go-around -> balloon). */
                int onapproach = along < -100.0;   /* behind the threshold: descend the glideslope by along-track
                   from far out, with the lateral pursuit centring CONCURRENTLY — so it arrives on-slope AND
                   centred instead of holding pattern altitude until aligned late and then diving (fast, steep). */
                if(onapproach && (st.elev-m.ld.elev) < clr+40.0) committed=1;
                tgt_alt = (onapproach||committed) ? gslope : pattern_alt;
            }
        }
        /* iNav fw_nav heading->roll (navigation_fixedwing.c updatePositionHeadingController_FW): a PID
           (fw_nav = pos_xy gains, D from error) on the COURSE error — measured over the GROUND track (COG,
           wind-robust), not yaw — outputs the roll angle, clamped to ±bank, then the control_smoothness roll
           LPF. navPidApply2 is iNav's OWN extracted function. cog+err vs cog cancels to error=headErr. */
        double desCourse = desHdg>-900.0? desHdg : bearing(st.lat,st.lon,tgt_la,tgt_lo);
        double cog = atan2(st.vx,-st.vz)*R2D; if(cog<0)cog+=360;       /* course over ground (X-Plane vx=E, vz=S) */
        double headErr = wrap180(desCourse-cog);
        /* iNav applies PID_SHRINK_INTEGRATOR while the heading error is CONVERGING (anti-windup on turn-in),
           evaluated at iNav's 2 Hz monitoring rate (navigation_fixedwing.c:534). Replicate it so the fw_nav
           integrator behaves like the real call site, not just the extracted PID function. */
        monTimer += dt;
        if(monTimer >= 0.5){ errDecr = fabs(prevHeadErr) > fabs(headErr); prevHeadErr = headErr; monTimer = 0.0; }
        int navFlags = PID_DTERM_FROM_ERROR | (errDecr ? PID_SHRINK_INTEGRATOR : 0);
        double bank_cmd = navPidApply2(&pidNav, headErr*100.0, 0.0, dt, -m.bank*100.0, m.bank*100.0, navFlags) / 100.0;
        { double cut=rcut, rc=1.0/(2*M_PI*cut), a=dt/(rc+dt);   /* iNav control_smoothness on the roll cmd (NAV_FW_BASE_ROLL_CUTOFF, base 10) */
          if(!rollLpfInit){rollLpf=bank_cmd;rollLpfInit=1;} rollLpf+=a*(bank_cmd-rollLpf); bank_cmd=rollLpf; }
        /* energy/launch bank caps (validator additions, applied ON TOP of iNav's ±bank clamp): a hard bank at
           low speed-margin bleeds a relaxed-stability jet into a departure; climb-out stays wings-level. */
        double smarg=(st.speed-m.vmin)/fmax(1.0,vtarget*1.3-m.vmin);
        double bankMax=m.bank*fmax(0.25,fmin(1.0,smarg));
        /* FLARE latch: once low on final, COMMIT — sticky through any balloon/bounce until touchdown, so a
           bounce never re-arms the pursuit/throttle into a go-around that departs. Arrests sink (pitch, above),
           holds wings level, cuts the engine (below). */
        double flare_h = clr + fmax(4.0, (st.speed - 1.2*m.vs)*0.8);            /* flare height scales with the speed to bleed: a fast approach (a motor-glider at cruise thrust) holds off higher/longer; a powered aircraft near approach speed flares low so it doesn't float */
        if(landing && !rolling && (st.elev-m.ld.elev) < flare_h) flaring=1;
        if(!launched) bankMax=fmin(bankMax,10.0);
        if(landing) bankMax=fmin(bankMax,25.0);                                  /* approach: gentle procedure-turn bank, stay well inside the bank envelope */
        if(flaring) bankMax=fmin(bankMax,6.0);                                   /* committed flare: wings-level for touchdown, don't cartwheel on a wingtip */
        bank_cmd=fmax(-bankMax,fmin(bankMax,bank_cmd));
        (void)tgt_spd;
        /* --- iNav fixed-wing altitude cascade (navigation_fixedwing.c): altitude error -> desired climb rate
           (alt_control_response, capped at max_auto_climb_rate), then the fw_alt PID on climb-rate error ->
           target pitch angle, clamped to [-dive, climb]. This is iNav's own law, not a stand-in. --- */
        double altErr_cm=(tgt_alt-st.elev)*100.0;
        double pitch_cmd;
        if(!launched){
            /* climb-out at the nav climb ANGLE directly (bypassing the nav auto-climb-RATE limit, which a
               fast airframe overruns — the rate PID would then command nose-down and upset a relaxed-
               stability jet). NOT nav_fw_launch_climb_angle: that is the bungee/hand-launch angle for a
               high-energy launch; the validator takes off ROLLING from a runway. */
            pitch_cmd = m.climb;
        } else {
            /* iNav fixed-wing altitude cascade (navigation_fixedwing.c): altitude error -> desired climb rate
               (alt_control_response, capped at max_auto_climb_rate), then the fw_alt PID on climb-rate error
               -> target pitch angle, clamped to [-dive, climb]. This is iNav's own law. */
            double desClimb=m.alt_response*altErr_cm/100.0;             /* cm/s */
            desClimb=fmax(-m.max_climb_rate,fmin(m.max_climb_rate,desClimb));
            double actClimb=st.vy*100.0;                               /* cm/s, +up */
            double pitch_dd=navPidApply2(&pidAlt,desClimb,actClimb,dt,-m.dive*10.0,m.climb*10.0,0);
            /* iNav pitch-command LPF (control_smoothness), cutoff floored: iNav's heaviest smoothing (~0.1 Hz)
               lags too far for this sim's phugoid and amplifies it, so clamp the cutoff to keep it a damper. */
            double cut=fmax(pcut,0.8), rc=1.0/(2*M_PI*cut), a=dt/(rc+dt);
            if(!pitchLpfInit){pitchLpf=pitch_dd;pitchLpfInit=1;} pitchLpf+=a*(pitch_dd-pitchLpf);
            pitch_cmd=fmax(-m.dive,fmin(m.climb,pitchLpf/10.0));        /* deg, reconstrained */
        }
        if(flaring) pitch_cmd = fmax(pitch_cmd, fmin(8.0, (0.4 - st.vy)*1.9));   /* hold off: arrest the sink and bleed speed for a slow touchdown (the sticky flare latch keeps a brief balloon wings-level + engine-off, so a firm hold-off is safe) */
        /* energy management: never demand more climb than airspeed allows — below the min MANEUVERING speed
           trade climb for speed (nose down) so a low-excess-thrust / relaxed-stability airframe never bleeds
           into a departure. The floor is max(vmin, 0.85*vc): a jet climbing+turning to a WP bleeds fast and
           mushes long before 1.2*Vs, so it must protect a near-cruise speed (a Cessna never gets that slow
           enroute, so this is inert for it). */
        double vmaneuver = fmax(m.vmin, 0.85*m.cruise);
        if(st.speed<vmaneuver && agl>30.0) pitch_cmd=fmin(pitch_cmd,(st.speed-vmaneuver)*0.2);  /* gentle: level off / mild descent when slow, don't dive hard (a jet on the backside can't recover a steep dive near the ground) */
        /* --- iNav throttle: cruise + pitch*pitch2thr (us). SITL iNav has no airspeed (min-speed uses GPS
           groundspeed @ 7 m/s), so it won't hold speed — a low-excess-thrust airframe (the F-16 at nav
           cruise) mushes off the back of the power curve in climbs/turns. Flightbox flies it with a MANUAL
           throttle hand (the "full-ANGLE climb" recipe); we model that: push throttle up toward max when
           airspeed falls below the nav reference. Near/above cruise this is ~0 (light aircraft unaffected). */
        /* throttle = iNav cruise + pitch2thr, plus a SPEED-HOLD toward a target airspeed. SITL iNav can't
           hold speed; flightbox flies the jet with a throttle hand that keeps it in its controllable band
           (above departure, below the high-q PIO regime). vtarget sits at the airframe's real operating
           speed (nav-reference cruise, or above the departure margin for a relaxed-stability jet). */
        /* speed target: operating speed enroute; on the approach BLEED to ~1.3·Vs (textbook approach speed,
           not below the min-maneuvering vmin) so touchdown is at approach speed, not a cruise-speed float. */
        double vspeed = landing ? fmax(m.vs*1.3, m.vmin) : vtarget;
        double sperr=vspeed-st.speed;
        double thr_us=m.cruise_thr + pitch_cmd*m.pitch2thr + (sperr>0? sperr*40.0 : (landing? sperr*12.0 : 0.0));  /* boost hard when slow; cut when fast ONLY on the approach — enroute let a jet fly at its natural cruise (its thrust/drag speed is well above the low iNav nav-reference airspeed; forcing it down to vc cuts power and mushes it). vne protection (below) still caps the top. */
        if(altErr_cm>3000.0) thr_us=fmax(thr_us, m.min_thr+0.7*(m.max_thr-m.min_thr));  /* sustained climb -> hold power up */
        if(!landing && agl>30.0 && jet && st.speed<m.vne*0.85) thr_us=fmax(thr_us,m.cruise_thr+0.4*(m.max_thr-m.cruise_thr)); /* JETS ONLY: a relaxed-stability jet at low power decays into departure. Conventional aircraft fly iNav's real nav_fw_cruise_thr + pitch2thr law (no power floor) — the floor overpowered them ~30% above cruise and made the validator OVER-PREDICT vs real iNav/flightbox (sim-critic). */
        if(st.speed>m.vne*0.9) thr_us=fmin(thr_us,m.min_thr);          /* vne protection (the only speed ceiling) */
        thr_us=fmax(m.min_thr,fmin(m.max_thr,thr_us));
        double thr=(thr_us-1000.0)/1000.0;
        if(flaring) thr=0.0;                                           /* engine OFF in the committed flare (bypasses the in-flight idle floor min_thr) — settle onto the runway, and let a motor-glider bleed to touchdown speed instead of idling in at cruise thrust */
        /* --- inner: iNav ANGLE mode + FW rate PID (the airframe's real stabilizer, per-aircraft gains) --- */
        double roll = fw_inner(&axRoll, bank_cmd, st.roll, st.p, m.fwPr,m.fwIr,m.fwDr,m.fwFfr, m.pLevel,m.iLevel, 200.0, dt);
        double pitch= fw_inner(&axPitch,pitch_cmd,st.pitch,st.q, m.fwPp,m.fwIp,m.fwDp,m.fwFfp, m.pLevel,m.iLevel, 200.0, dt);
        /* FDM ground under the aircraft: the landing field once on approach, else takeoff. Takeoff and
           landing airports differ in elevation -> AGL for crash/stall/touchdown must be vs the LOCAL ground,
           while launch/WP altitudes stay home-relative (takeoff). */
        double ground = landing ? m.ld.elev : m.to.elev;
        /* rudder = 0: iNav's TURN_ASSIST (fw_p_yaw/fw_turn_assist_yaw_gain) rotates the rate SETPOINTS into
           the earth frame; a naive coordinating rudder destabilises the FDM. The models' natural yaw coupling
           already tracks the coordinated turn rate within ~5-15%, so rudder is left at 0 (see ringtrack.ts
           INAV_ACK_NOT_APPLIED) pending a faithful port — an honest gap, not a silent one. */
        double yaw = 0.0;
        (void)m.fwPy; (void)m.turnAssist;
        /* ROLLOUT after touchdown: cut the engine, brake hard, and steer (nose-wheel via rudder) to hold the
           runway centreline until the aircraft stops — a landing is not done at wheels-down, it is done at a
           full stop ON the runway. */
        double brake_cmd = 0.0;
        if(rolling && (st.elev-ground) < clr+2.0){          /* hold the rollout override (wings-level + brakes)
                                                               THROUGH small touchdown bounces — resuming the
                                                               COG-based nav at bounce speed cartwheels it */
            thr=0.0; brake_cmd=0.4; roll=0.0; pitch=0.10;   /* engine cut, brakes; slight nose-up = aero-braking + keep the nose light */
            double hr=m.ld.hdg*D2R, fe=sin(hr), fn=cos(hr), e,n;
            enu(st.lat,st.lon,m.ld.lat,m.ld.lon,&e,&n);
            double xte=e*(-fn)+n*fe;
            yaw=fmax(-1.0,fmin(1.0, -xte*0.05 - st.r*0.02));   /* steer to centreline + damp */
        }
        fb_jsbsim_set_ground(ground);
        fb_jsbsim_set_brake(brake_cmd);
        fb_jsbsim_set_controls(roll,pitch,yaw,thr);
        fb_jsbsim_step(&st);
        if(getenv("VTRACE") && ((int)(t/dt)%500==0))
            fprintf(stderr,"t=%5.1f agl=%6.1f spd=%5.1f vz=%+5.1f roll=%+5.1f pitch=%+5.1f | bank_cmd=%+5.1f pit_cmd=%+5.1f thr=%.2f ph=%s%d wp=%d dth=%.0f tgtalt=%.0f\n",
                t,agl,st.speed,st.vy,st.roll,st.pitch,bank_cmd,pitch_cmd,thr,!launched?"LNCH":landing?"LAND":"WP",landphase,wp,distll(st.lat,st.lon,m.ld.lat,m.ld.lon),tgt_alt-m.ld.elev);
        if(ntraj<MAXTRAJ){ traj[ntraj].lat=st.lat; traj[ntraj].lon=st.lon; traj[ntraj].asl=st.elev; ntraj++; }
        if(getenv("VOUT") && ((int)(t/dt)%10==0)){       /* trajectory CSV @10 Hz for the TS predicate evaluator */
            double course=atan2(st.vx,-st.vz)*R2D; if(course<0)course+=360;   /* X-Plane vx=E, vz=S -> track */
            printf("%.2f %.7f %.7f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %d %s\n",
                t,st.lat,st.lon,st.elev,st.elev-ground,st.roll,st.pitch,st.yaw,st.speed,st.vy,st.gs,course,
                wp,!launched?"launch":landing?"land":"task"); }
        /* --- envelope guards (fail-fast) --- */
        if(!(isfinite(st.lat)&&isfinite(st.lon)&&isfinite(st.elev))){ snprintf(fail,sizeof fail,"NaN/divergence at t=%.1f",t); crashed=1; break; }
        agl=st.elev-m.to.elev;                                          /* home-relative (launch/WP datum) */
        double aglL=st.elev-ground;                                     /* local AGL (crash/stall/touchdown) */
        if(aglL < (rolling? -3.0 : -1.0)){ snprintf(fail,sizeof fail,"below ground (crash) at t=%.1f",t); crashed=1; break; }  /* on the gear the CG dips with strut compression — more margin during rollout */
        if(!landing && launched && aglL < clr+1.5 && t>8.0){ snprintf(fail,sizeof fail,"ground contact before landing (departure/crash) at t=%.1f",t); crashed=1; break; }
        if(!rolling && st.speed < m.vs*0.9 && aglL>5){ snprintf(fail,sizeof fail,"stalled: TAS %.1f < %.1f at t=%.1f",st.speed,m.vs*0.9,t); crashed=1; break; }
        if(!rolling && launched && st.speed < m.vs*0.7 && t>10.0){ snprintf(fail,sizeof fail,"departed/mushed: TAS %.1f < %.1f at t=%.1f",st.speed,m.vs*0.7,t); crashed=1; break; }
        if(!rolling && st.speed > m.vne){ snprintf(fail,sizeof fail,"overspeed: TAS %.1f > vne %.1f at t=%.1f",st.speed,m.vne,t); crashed=1; break; }  /* on the ground TAS spikes on contact — GS governs the rollout */
        /* --- WP capture / touchdown (the flightbox orakel: reach each WP in the capture radius, land near
           the threshold; envelope must hold throughout) --- */
        /* iNav's REAL fixed-wing waypoint-reached (navigation.c isWaypointReached): the WP is reached when the
           bearing to it swings >100° past the bearing held when the leg became active (the aircraft has PASSED
           the waypoint), OR it comes within nav_wp_radius. This is DECOUPLED from the grading tolerance — the
           nav advances on iNav's own criterion, and wpmin[] is then an INDEPENDENT measurement of how close the
           flight actually passed (the runner grades that against a realistic accuracy tolerance, not this). */
        if(launched && wp<m.nwp){
            double dw=distll(st.lat,st.lon,tgt_la,tgt_lo); if(dw<wpmin[wp])wpmin[wp]=dw;
            double brg=bearing(st.lat,st.lon,tgt_la,tgt_lo);
            if(wpBrgSet!=wp){ wpInitBrg=brg; wpBrgSet=wp; }
            int passed = fabs(wrap180(brg-wpInitBrg)) > 100.0;   /* relativeBearingTargetAngle = 100° (iNav default) */
            if(dw < NAV_WP_RADIUS_M || passed) wp++;
        }
        if(landing){ double dl=distll(st.lat,st.lon,m.ld.lat,m.ld.lon);
            if(aglL<clr+3.0 && dl<tdist) tdist=dl;
            if(!rolling && aglL<clr+2.0){                  /* TOUCHDOWN — record how/where it met the runway (latch a touch above the wheels so a small bounce can't skip past the trigger and go around) */
                rolling=1; td_lat=st.lat; td_lon=st.lon; td_speed=st.gs; td_cas=st.cas; td_vz=st.vy;
                double hr=m.ld.hdg*D2R, fe=sin(hr), fn=cos(hr), e,n;
                enu(st.lat,st.lon,m.ld.lat,m.ld.lon,&e,&n);
                td_along=e*fe+n*fn;                        /* metres past the threshold along the runway */
                td_xte=e*(-fn)+n*fe;                       /* cross-track from the centreline */
                td_hdgerr=wrap180(st.yaw-m.ld.hdg);        /* heading vs runway (alignment) */
            }
            if(rolling){
                rollout=distll(td_lat,td_lon,st.lat,st.lon);
                if(st.gs<0.5){ landed=1; break; }          /* rolled to a full stop */
                if(td_along+rollout > m.ld.len+150.0){ snprintf(fail,sizeof fail,"runway overrun: %.0fm past a %.0fm runway",td_along+rollout,m.ld.len); crashed=1; break; }
            } }
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
    /* machine-readable landing metrics for the TS runner to grade (on-runway? aligned? soft? not hot? stopped?):
       along, cross-track, heading-error, groundspeed, rollout, runway-len, stopped, calibrated-airspeed, sink-rate */
    printf("LAND %.1f %.1f %.1f %.1f %.1f %.1f %d %.1f %.2f\n", td_along, td_xte, td_hdgerr, td_speed, rollout, m.ld.len, landed, td_cas, -td_vz);
    /* verdict: launched, every WP captured, and a COMPLETE landing — touched down ON the runway (aligned,
       on the strip) AND rolled to a full stop on it, envelope never violated. `stopped` = the rollout latched
       (gs -> 0); the runner grades WHERE on the runway + alignment + rollout distance from the metrics below. */
    double wpworst=0; for(int i=0;i<m.nwp;i++) if(wpmin[i]>wpworst) wpworst=wpmin[i];
    if(!crashed){
        if(!launched) snprintf(fail,sizeof fail,"never launched");
        else if(wp<m.nwp) snprintf(fail,sizeof fail,"WP%d missed (closest %.0fm)",wp,wpmin[wp]);
        else if(!rolling) snprintf(fail,sizeof fail,"no touchdown (closest to threshold %.0fm)",tdist);
        else if(!landed) snprintf(fail,sizeof fail,"never stopped on the runway (rolled %.0fm, still %.1f m/s)",rollout,st.gs);
    }
    int ok = !crashed && launched && wp>=m.nwp && landed;
    /* summary line. `rings` is DIAGNOSTIC ONLY (the fine climb/route/approach corridor gates, for eyeballing
       the trajectory) — it does not gate the verdict; the TS runner's CORRIDOR predicate is the corridor gate. */
    printf("%s ac=%s wp=%d/%d worst-capture=%.0fm td[along=%.0f xte=%.0f hdg=%+.0f gs=%.0f]m rollout=%.0f/%.0fm rings=%d/%d(diag) t=%.0fs%s%s\n",
           ok?"OK":"FAIL", m.ac, wp, m.nwp, wpworst, td_along, td_xte, td_hdgerr, td_speed, rollout, m.ld.len, inorder, ngate, t,
           fail[0]?"  ":"", fail);
    return ok?0:1;
}
