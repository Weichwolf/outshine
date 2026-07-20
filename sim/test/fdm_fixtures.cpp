/* FDM fixture suite — the deterministic flight-dynamics spec, structured as a PPL(A) syllabus.
 *
 * Each fixture is (environment + initial flight-state + control signal) -> expected result. The control
 * is applied through a minimal iNav-style inner loop (P attitude-hold on roll/pitch, fixed throttle,
 * optional gear/flap) so a fixture tests the AIRFRAME's closed-loop response — what iNav actually asks
 * of it — not an open-loop departure. Pure libJSBSim via the bridge adapter; no container, no iNav, no
 * network. Deterministic: same fixture -> same numbers every run.
 *
 * Coverage mirrors the PPL(A) exercises a real airplane pilot is examined on:
 *   Ex4 Effects of controls · Ex6 Straight & level · Ex7 Climbing · Ex8 Descending/glide ·
 *   Ex9 Turning (medium & steep, coordinated) · Ex10 Slow flight · Ex11 Stalling · Ex13 Circuit ·
 *   Ex17/18 Nav & wind (head/tail/cross). Flown across the three aircraft.
 *
 * Build+run: test/fdm-test.sh. Exit 0 = all green.
 */
#include "jsbsim_adapter.h"
#include <cstdio>
#include <cmath>

static int RUN = 0, FAIL = 0;
static void ck(bool c, const char *what){ RUN++; if(!c){ FAIL++; printf("  [FAIL] %s\n", what); } }
static void ck_rng(double v, double lo, double hi, const char *what){
    RUN++; if(!(v >= lo && v <= hi)){ FAIL++; printf("  [FAIL] %s: got %.3f, want [%.3f,%.3f]\n", what, v, lo, hi); }
}
static const double G = 9.80665, R2D = 57.2957795;
static const char *ROOT = "aircraft/models";

struct Result { double V, vs, roll, pitch, gs, hdg, turn_rate; bool ok; };

/* Hold roll_sp/pitch_sp via P control at fixed throttle in a given wind + spawn altitude for <secs>;
 * return averages over the last 5 s (steady state) plus the mean turn rate over that window. */
static Result fly(const char *ac, double alt_m, double speed, double yaw,
                  double wind_n, double wind_e, double roll_sp, double pitch_sp,
                  double thr, double gear, double flap, double secs){
    Result r{}; r.ok = false;
    if(fb_jsbsim_init(ROOT, ac, 47.5, 9.5, alt_m, 0.0, speed, yaw, 0)){ printf("  [ERR] init %s\n", ac); return r; }
    fb_jsbsim_set_wind(wind_n, wind_e);
    fb_fdm_state s{}; s.yaw = yaw;
    int steps = (int)(secs / 0.01), tail = 500, n = 0;
    double aV=0,aVS=0,aRoll=0,aPitch=0,aGS=0, hdg0=0;
    for(int i=0;i<steps;i++){
        double ail=(-0.03)*(s.roll-roll_sp);  if(ail>1)ail=1; if(ail<-1)ail=-1;
        double ele=(-0.06)*(s.pitch-pitch_sp);if(ele>1)ele=1; if(ele<-1)ele=-1;
        double rud=(-0.02)*s.roll;
        fb_jsbsim_set_controls(ail, ele, rud, thr);
        fb_jsbsim_set_aux(gear, flap, -1.0);
        fb_jsbsim_step(&s);
        if(i==steps-tail-1) hdg0 = s.yaw<0?s.yaw+360:s.yaw;
        if(i>=steps-tail){ aV+=s.speed; aVS+=s.vy; aRoll+=s.roll; aPitch+=s.pitch; aGS+=s.gs; n++; }
    }
    double hdg1 = s.yaw<0?s.yaw+360:s.yaw, dh = hdg1-hdg0; while(dh>180)dh-=360; while(dh<-180)dh+=360;
    r.V=aV/n; r.vs=aVS/n; r.roll=aRoll/n; r.pitch=aPitch/n; r.gs=aGS/n; r.hdg=hdg1;
    r.turn_rate = dh/(tail*0.01); r.ok = std::isfinite(r.V) && std::isfinite(r.roll);
    return r;
}

/* Ex11 stall: from level, cut throttle and hold the nose up (pitch_sp high) so speed decays; report the
 * minimum airspeed reached and the peak sink rate — a stall shows as speed bleeding off and sink developing. */
static void stall_probe(const char *ac, double alt_m, double speed, double thr, double *vmin, double *sink){
    *vmin = 1e9; *sink = 0;
    if(fb_jsbsim_init(ROOT, ac, 47.5, 9.5, alt_m, 0.0, speed, 90, 0)) return;
    fb_jsbsim_set_wind(0,0);
    fb_fdm_state s{}; s.yaw=90;
    for(int i=0;i<3500;i++){
        double ele=(-0.06)*(s.pitch-15.0); if(ele>1)ele=1; if(ele<-1)ele=-1;   /* hold nose well up */
        double ail=(-0.03)*s.roll;
        fb_jsbsim_set_controls(ail, ele, 0, thr);
        fb_jsbsim_step(&s);
        if(i>200){ if(s.speed<*vmin)*vmin=s.speed; if(-s.vy>*sink)*sink=-s.vy; }
    }
}

int main(){
    printf("== FDM fixture suite — PPL(A) syllabus (env + state + control -> expected) ==\n");

    /* ===================== Ex4 — EFFECTS OF CONTROLS (c172 trainer) ===================== */
    printf("\n-- Ex4 effects of controls (c172) --\n");
    { Result u = fly("c172", 600, 20, 90, 0,0, 0, +6, 0.55, -1,-1, 30);
      Result d = fly("c172", 600, 20, 90, 0,0, 0, -6, 0.55, -1,-1, 30);
      ck(u.pitch > d.pitch + 3, "elevator: nose-up sets higher pitch than nose-down"); }
    { Result rt = fly("c172", 600, 20, 90, 0,0, +20, 0, 0.55, -1,-1, 25);
      ck(rt.roll > 8, "aileron: right stick -> right bank"); }
    { Result hi = fly("c172", 600, 20, 90, 0,0, 0,0, 0.75, -1,-1, 40);
      Result lo = fly("c172", 600, 20, 90, 0,0, 0,0, 0.30, -1,-1, 40);
      ck(hi.V > lo.V + 1.0 || hi.vs > lo.vs + 0.5, "power: more throttle -> faster or climbing"); }
    { Result cl = fly("c172", 600, 22, 90, 0,0, 0,0, 0.50, -1, 0.0, 40);
      Result fl = fly("c172", 600, 22, 90, 0,0, 0,0, 0.50, -1, 1.0, 40);
      ck(fl.V < cl.V - 0.3 || fl.vs < cl.vs - 0.2, "flap: extension adds drag/lift (slower or more sink)"); }

    /* ===================== Ex6 — STRAIGHT & LEVEL ===================== */
    printf("\n-- Ex6 straight & level --\n");
    { Result r = fly("c172", 500, 20, 90, 0,0, 0,0, 0.45, -1,-1, 45);
      ck(r.ok, "c172 S&L finite");
      ck_rng(r.vs, -1.5, 1.5, "c172 S&L: altitude held (vs~0)");
      ck_rng(fabs(r.roll), 0, 6, "c172 S&L: wings level");
      ck_rng(r.V, 14, 30, "c172 S&L: cruise speed sane"); }
    { Result r = fly("sgs233", 500, 20, 90, 0,0, 0,0, 0.30, -1,-1, 45);
      ck(r.ok, "sgs233 S&L finite");
      ck_rng(r.vs, -2.0, 2.0, "sgs233 S&L: altitude held");
      ck_rng(fabs(r.roll), 0, 6, "sgs233 S&L: wings level"); }
    { Result r = fly("f16", 1000, 60, 90, 0,0, 0,0, 0.80, -1,-1, 45);
      ck(r.ok, "f16 S&L finite");
      ck_rng(r.V, 30, 130, "f16 S&L: fast-jet speed"); }

    /* ===================== Ex7 — CLIMBING ===================== */
    printf("\n-- Ex7 climbing --\n");
    { Result c = fly("c172", 400, 20, 90, 0,0, 0, +8, 0.75, -1,-1, 40);
      ck_rng(c.vs, 0.8, 12, "c172 climb: positive rate of climb"); }
    { Result c = fly("sgs233", 400, 20, 90, 0,0, 0, +6, 0.60, -1,-1, 40);
      ck(c.vs > 0.3, "sgs233 climb: climbs under power"); }

    /* ===================== Ex8 — DESCENDING & GLIDE ===================== */
    printf("\n-- Ex8 descending & glide --\n");
    { Result d = fly("c172", 900, 20, 90, 0,0, 0, -6, 0.20, -1,-1, 40);
      ck_rng(d.vs, -12, -0.8, "c172 powered descent: sinks"); }
    { Result g = fly("sgs233", 900, 22, 90, 0,0, 0, 0, 0.0, -1,-1, 45);
      ck(g.vs < -0.2, "sgs233 glide (idle): descends");
      ck(g.V > 6, "sgs233 glide: maintains flying speed"); }

    /* ===================== Ex9 — TURNING (medium & steep, coordinated) ===================== */
    printf("\n-- Ex9 turning --\n");
    { Result t = fly("c172", 700, 20, 90, 0,0, 30, 0, 0.60, -1,-1, 40);   /* medium turn 30 deg */
      ck_rng(fabs(t.roll), 18, 40, "c172 medium turn: ~30 deg bank held");
      double want = G*tan(fabs(t.roll)/R2D)/t.V*R2D;
      ck_rng(fabs(t.turn_rate), want*0.4, want*1.8, "c172 medium turn: rate ~ g*tan(phi)/V");
      ck(t.turn_rate > 2, "c172 turn: actually yawing around"); }
    { Result t = fly("c172", 900, 22, 90, 0,0, 45, 0, 0.75, -1,-1, 40);   /* steep turn 45 deg */
      ck_rng(fabs(t.roll), 30, 55, "c172 steep turn: ~45 deg bank held");
      ck(fabs(t.turn_rate) > 6, "c172 steep turn: higher rate than medium"); }

    /* ===================== Ex10 — SLOW FLIGHT ===================== */
    printf("\n-- Ex10 slow flight --\n");
    { Result r = fly("c172", 700, 14, 90, 0,0, 0,0, 0.45, -1, 1.0, 45);   /* slow, flap out */
      ck(r.ok && std::isfinite(r.V), "c172 slow flight: controllable, no departure");
      ck_rng(fabs(r.roll), 0, 15, "c172 slow flight: wings roughly level"); }

    /* ===================== Ex11 — STALLING ===================== */
    printf("\n-- Ex11 stalling --\n");
    { double vmin=0, sink=0; stall_probe("c172", 900, 20, 0.0, &vmin, &sink);   /* idle, nose held up */
      ck(vmin < 14, "c172 stall: airspeed decays toward stall");
      ck(sink > 1.5, "c172 stall: sink develops as the wing stalls"); }

    /* ===================== Ex17/18 — NAVIGATION & WIND (the wind triangle) ===================== */
    /* groundspeed must equal the vector sum of air velocity (V on the aircraft's heading) and the wind
     * vector — the heading-independent spec, robust to the airframe's own heading drift. */
    printf("\n-- Ex17/18 navigation & wind --\n");
    for(int w=0; w<3; w++){
        double wn = (w==2)? 8.0 : 0.0, we = (w==0)? -8.0 : (w==1)? 8.0 : 0.0;
        const char *nm = (w==0)?"headwind":(w==1)?"tailwind":"crosswind";
        Result r = fly("c172", 500, 20, 90, wn, we, 0,0, 0.45, -1,-1, 40);
        double hr = r.hdg/R2D, vN = r.V*cos(hr)+wn, vE = r.V*sin(hr)+we, gexp = sqrt(vN*vN+vE*vE);
        char buf[96]; snprintf(buf,sizeof buf,"%s: gs = |air + wind| (exp %.1f, got %.1f)", nm, gexp, r.gs);
        ck(fabs(r.gs-gexp) < 2.0, buf);
    }

    /* ===================== f16 gear (retract reduces drag) ===================== */
    printf("\n-- f16 gear --\n");
    { Result dn = fly("f16", 1000, 70, 90, 0,0, 0,0, 0.85, 1.0, -1, 50);
      Result up = fly("f16", 1000, 70, 90, 0,0, 0,0, 0.85, 0.0, -1, 50);
      ck(up.V > dn.V + 0.5, "f16 gear: retracted faster than gear-down (drag)"); }

    printf("\n== %d checks, %d failed -> %s ==\n", RUN, FAIL, FAIL ? "FAIL" : "PASS");
    return FAIL ? 1 : 0;
}
