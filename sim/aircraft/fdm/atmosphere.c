/* FlightBox FDM — atmosphere. See atmosphere.h for the contract. */
#include "atmosphere.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void fb_atmo_init(fb_atmo *a){
    a->windN = a->windE = 0.0;
    a->gustN = a->gustE = 0.0;
    a->turb = 1.0; a->sigma = 0.6; a->bl_height = 800.0; a->thermal_W = 0.0;
    a->windN_t = a->windE_t = 0.0;
    a->turb_t = 1.0; a->sigma_t = 0.6; a->bl_t = 800.0; a->thermal_t = 0.0;
    a->first = 1;
    a->gustP = a->gustQ = 0.0;
    a->wg = a->pg = a->qg = 0.0;
    a->rng = 2463534242u;
}

double fb_atmo_urand(fb_atmo *a){
    a->rng ^= a->rng << 13; a->rng ^= a->rng >> 17; a->rng ^= a->rng << 5;
    return a->rng / 4294967296.0;
}

/* ~gaussian, mean 0, UNIT variance (sum of 12 uniforms - 6). The old sum-of-4 had variance
 * 1/3, so every turbulence RMS driven by it was 0.58x the commanded sigma. */
double fb_atmo_nrand(fb_atmo *a){
    double s = 0; for(int i = 0; i < 12; i++) s += fb_atmo_urand(a);
    return s - 6.0;
}

/* --- thermals: a field of Gaussian updraft columns on a jittered grid. Strength and top come
 * from live weather (solar radiation + boundary-layer height). The plane bobs through lift and
 * gentle inter-thermal sink, like a real soaring day. */
static uint32_t hash2(int a, int b){
    uint32_t h = (uint32_t)(a*73856093) ^ (uint32_t)(b*19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h;
}

double fb_atmo_thermal_w(const fb_atmo *a, double x, double y, double z){
    if(a->thermal_W < 0.1) return 0.0;
    const double cell = 340.0, R = 110.0;              /* thermal spacing + core radius, m */
    double zf = z / fmax(a->bl_height, 200.0);
    if(zf <= 0.0 || zf >= 1.0) return -0.10*a->thermal_W;  /* below/above the convective layer */
    double prof = sin(zf*M_PI);                        /* 0 at ground, peak mid-layer, 0 at BL top */
    int gx = (int)floor(x/cell + 0.5), gy = (int)floor(y/cell + 0.5);
    double best = 0.0;
    for(int dx = -1; dx <= 1; dx++) for(int dy = -1; dy <= 1; dy++){
        uint32_t h = hash2(gx+dx, gy+dy);
        double cx = (gx+dx)*cell + ((h&255)/255.0 - 0.5)*cell*0.6;
        double cy = (gy+dy)*cell + (((h>>8)&255)/255.0 - 0.5)*cell*0.6;
        double rr = hypot(x-cx, y-cy);
        double str = a->thermal_W*(0.55 + 0.45*(((h>>16)&255)/255.0));
        double w = str*exp(-(rr*rr)/(R*R));
        if(w > best) best = w;
    }
    return best*prof - 0.12*a->thermal_W;              /* core lift minus gentle ambient sink */
}

void fb_atmo_set_target(fb_atmo *a, double windN, double windE,
                        double turb, double sigma, double bl_height, double thermal_W){
    a->windN_t = windN; a->windE_t = windE;
    a->turb_t = turb; a->sigma_t = sigma; a->bl_t = bl_height; a->thermal_t = thermal_W;
}

void fb_atmo_snap(fb_atmo *a){
    a->windN = a->windN_t; a->windE = a->windE_t;
    a->turb = a->turb_t; a->sigma = a->sigma_t;
    a->bl_height = a->bl_t; a->thermal_W = a->thermal_t;
    a->first = 0;
}

void fb_atmo_slew(fb_atmo *a, double dt){
    if(a->first){ fb_atmo_snap(a); return; }       /* first observation applies at once */
    double k = dt / (FB_ATMO_TAU + dt);            /* exponential ease, stable for any dt>0 */
    a->windN     += k*(a->windN_t   - a->windN);
    a->windE     += k*(a->windE_t   - a->windE);
    a->turb      += k*(a->turb_t    - a->turb);
    a->sigma     += k*(a->sigma_t   - a->sigma);
    a->bl_height += k*(a->bl_t      - a->bl_height);
    a->thermal_W += k*(a->thermal_t - a->thermal_W);
}

/* --- Dryden-form turbulence (MIL-F-8785C low-altitude spectra). wg = vertical gust (m/s, tilts
 * the local alpha -> a REAL aero disturbance); pg/qg = roll/pitch buffet rates reported to
 * iNav's gyro so its rate loop can fight them. */
void fb_atmo_step_gusts(fb_atmo *a, double dt, double agl, double speed){
    double Vrep = fmax(speed, 3.0);                /* turbulence scale-lengths need a speed */
    double h = fmax(agl, 10.0), hf = h/0.3048;
    double Lw = fmax(h, 30.0), Lu = 0.3048*fmax(hf, 50.0)/pow(0.177 + 0.000823*hf, 1.2);
    double aw = fmin(1.0, Vrep*dt/Lw), au = fmin(1.0, Vrep*dt/fmax(Lu, 20.0));
    double gsc = fmin(1.0, 0.30 + 0.012*agl)*a->sigma;    /* ease near ground for climb-out */
    a->wg += -aw*a->wg + gsc     *sqrt(2*aw)*fb_atmo_nrand(a);   /* vertical gust, m/s */
    a->pg += -au*a->pg + 3.5*gsc *sqrt(2*au)*fb_atmo_nrand(a);   /* roll-rate gust, deg/s */
    a->qg += -au*a->qg + 1.7*gsc *sqrt(2*au)*fb_atmo_nrand(a);   /* pitch-rate gust, deg/s */
    a->gustP = a->pg; a->gustQ = a->qg;                   /* -> iNav gyro (P/Q drefs) */
    /* Ornstein-Uhlenbeck horizontal wind wander (mean-revert to 0 offset) */
    double gtau = 3.0, gsig = 0.52*a->turb;
    a->gustN += (-a->gustN/gtau)*dt + gsig*sqrt(dt)*fb_atmo_nrand(a);
    a->gustE += (-a->gustE/gtau)*dt + gsig*sqrt(dt)*fb_atmo_nrand(a);
}
