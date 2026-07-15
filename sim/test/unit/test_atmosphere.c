/* Unit tests — fdm/atmosphere.c
 *
 * This maths used to be file-scope statics inside xp_bridge.c, reachable only by flying the whole
 * closed loop. Extracted into a struct, it can be asserted directly: a thermal's shape, a gust's
 * RMS, and the wind slew that this project documented for months without ever implementing.
 *
 * The random paths are tested statistically (mean/variance/RMS over many samples), not against
 * golden numbers: the RNG is deterministic, but pinning exact draws would test the xorshift
 * constants rather than the physics, and would fight every future reorder of the call sites.
 */
#include "tassert.h"
#include "../../aircraft/fdm/atmosphere.h"
#include <math.h>

void test_atmosphere(void){
    tsection("atmosphere: init + RNG");
    {
        fb_atmo a; fb_atmo_init(&a);
        ck(a.turb == 1.0 && a.sigma == 0.6, "init: moderate turbulence by default");
        ck(a.bl_height == 800.0 && a.thermal_W == 0.0, "init: BL 800 m, thermals off");
        ck(a.windN == 0.0 && a.windE == 0.0 && a.gustN == 0.0, "init: still air");
        ck(a.first == 1, "init: first observation snaps rather than ramping");

        /* uniform in [0,1) and actually uniform-ish */
        double lo = 1e9, hi = -1e9, sum = 0; int n = 20000;
        for(int i = 0; i < n; i++){ double u = fb_atmo_urand(&a); if(u<lo)lo=u; if(u>hi)hi=u; sum+=u; }
        ck(lo >= 0.0 && hi < 1.0, "urand stays in [0,1)");
        ck_near(sum/n, 0.5, 0.02, "urand mean ~ 0.5");

        /* nrand must have UNIT variance — a sum-of-4 version once made every turbulence
         * RMS 0.58x the commanded sigma, which is exactly the bug this pins. */
        fb_atmo b; fb_atmo_init(&b);
        double s = 0, ss = 0; n = 40000;
        for(int i = 0; i < n; i++){ double g = fb_atmo_nrand(&b); s += g; ss += g*g; }
        ck_near(s/n, 0.0, 0.03, "nrand mean ~ 0");
        ck_near(ss/n, 1.0, 0.05, "nrand VARIANCE ~ 1 (not 1/3 — sigma must mean sigma)");

        /* determinism: same seed, same stream */
        fb_atmo c, d; fb_atmo_init(&c); fb_atmo_init(&d);
        int same = 1; for(int i = 0; i < 500; i++) if(fb_atmo_urand(&c) != fb_atmo_urand(&d)) same = 0;
        ck(same, "same seed -> identical stream (runs stay reproducible)");
    }

    tsection("atmosphere: thermal field");
    {
        fb_atmo a; fb_atmo_init(&a);
        a.thermal_W = 0.0;
        ck(fb_atmo_thermal_w(&a, 0, 0, 400) == 0.0, "thermals off -> exactly zero, no sink");
        a.thermal_W = 0.05;
        ck(fb_atmo_thermal_w(&a, 0, 0, 400) == 0.0, "below the 0.1 threshold -> still off");

        a.thermal_W = 3.0; a.bl_height = 1000.0;
        ck(fb_atmo_thermal_w(&a, 0, 0, -5) < 0.0,   "below ground level -> ambient sink, no lift");
        ck(fb_atmo_thermal_w(&a, 0, 0, 1200) < 0.0, "above the BL top -> ambient sink, no lift");
        ck_near(fb_atmo_thermal_w(&a, 0, 0, 1200), -0.10*3.0, 1e-12, "out-of-layer sink = -0.10*W");

        /* somewhere in a 3x3 km patch at mid-layer there must be real lift */
        double best = -100;
        for(double x = -1500; x <= 1500; x += 25) for(double y = -1500; y <= 1500; y += 25){
            double w = fb_atmo_thermal_w(&a, x, y, 500); if(w > best) best = w;
        }
        ck(best > 1.0, "a core somewhere in the field gives real lift (>1 m/s)");
        ck(best <= 3.0, "no core exceeds the commanded peak W");

        /* the sin() profile: same column, weakest at the edges of the layer */
        double wmid = fb_atmo_thermal_w(&a, 0, 0, 500);
        double wlow = fb_atmo_thermal_w(&a, 0, 0, 20);
        double whigh= fb_atmo_thermal_w(&a, 0, 0, 980);
        ck(wmid > wlow && wmid > whigh, "lift peaks mid-layer, fades to ground and BL top");

        /* mean over the field must be near zero-ish: lift is compensated by sink, not free energy */
        double sum = 0; int n = 0;
        for(double x = -2000; x <= 2000; x += 20) for(double y = -2000; y <= 2000; y += 20){
            sum += fb_atmo_thermal_w(&a, x, y, 500); n++;
        }
        ck(fabs(sum/n) < 0.5, "field-average vertical motion stays small (lift ~ balanced by sink)");
        ck(a.thermal_W == 3.0 && a.bl_height == 1000.0, "thermal_w is pure: it mutates nothing");
    }

    tsection("atmosphere: weather targets + slew");
    {
        fb_atmo a; fb_atmo_init(&a);
        fb_atmo_set_target(&a, 10.0, -4.0, 1.5, 0.9, 1200.0, 2.5);
        ck(a.windN_t == 10.0 && a.windE_t == -4.0, "set_target records the wind target");
        ck(a.windN == 0.0 && a.windE == 0.0, "set_target alone does NOT move the current wind");
        fb_atmo_slew(&a, 0.01);
        ck(a.windN < 1.0 && a.windN > 0.0, "set_target + slew ramps; it does not snap");

        /* fb_atmo_observe: the FIRST live observation applies at once. `first` must be consumed
         * here and not in slew -- the FDM slews every tick, so a flag checked there would be
         * spent on tick one and the first real weather would ramp out of an env-var guess. */
        fb_atmo b; fb_atmo_init(&b);
        fb_atmo_observe(&b, 10.0, -4.0, 1.5, 0.9, 1200.0, 2.5);
        ck(b.windN == 10.0 && b.windE == -4.0, "the FIRST observation applies instantly");
        ck(b.turb == 1.5 && b.sigma == 0.9 && b.bl_height == 1200.0 && b.thermal_W == 2.5,
           "the first observation snaps every field, not just wind");
        ck(b.first == 0, "the snap is one-shot");

        /* every LATER observation ramps -- this is the 15-minute jolt this module exists to kill */
        fb_atmo_observe(&b, -10.0, 4.0, 0.3, 0.2, 400.0, 0.0);
        ck(b.windN == 10.0, "a later observation does not move the wind by itself");
        fb_atmo_slew(&b, 0.01);
        ck(fabs(b.windN - 10.0) < 0.01, "a 20 m/s wind reversal does NOT step instantly");
        ck(b.windN < 10.0, "...but it has started moving");

        /* slew must be safe to call every tick once we are already there */
        fb_atmo c; fb_atmo_init(&c);
        fb_atmo_observe(&c, 5.0, 0, 1, 0.6, 800, 0);
        for(int i = 0; i < 1000; i++) fb_atmo_slew(&c, 0.01);
        ck(c.windN == 5.0, "slew is a no-op once current == target (no drift from repeated calls)");

        /* after one time constant, an exponential ease has covered ~63% */
        fb_atmo d; fb_atmo_init(&d);
        fb_atmo_observe(&d, 10.0, 0, 1, 0.6, 800, 0);      /* first: snaps */
        fb_atmo_observe(&d, 0.0, 0, 1, 0.6, 800, 0);       /* second: ramps */
        for(double t = 0; t < FB_ATMO_TAU; t += 0.01) fb_atmo_slew(&d, 0.01);
        ck_near(d.windN, 10.0*exp(-1.0), 0.15, "after one tau, ~1/e of the change remains");
        for(double t = 0; t < 10*FB_ATMO_TAU; t += 0.01) fb_atmo_slew(&d, 0.01);
        ck(fabs(d.windN) < 0.01, "slew converges onto the target");

        /* a big dt must not overshoot past the target (k = dt/(tau+dt) < 1 always) */
        fb_atmo e; fb_atmo_init(&e);
        fb_atmo_observe(&e, 0, 0, 1, 0.6, 800, 0);
        fb_atmo_observe(&e, 10.0, 0, 1, 0.6, 800, 0);
        fb_atmo_slew(&e, 1e6);
        ck(e.windN <= 10.0 && e.windN > 9.0, "a huge dt converges, never overshoots");

        /* the jolt this replaces, quantified: at 100 Hz the wind must move by a hair, not a step */
        fb_atmo f; fb_atmo_init(&f);
        fb_atmo_observe(&f, 0, 0, 1, 0.6, 800, 0);
        fb_atmo_observe(&f, 12.0, 0, 1, 0.6, 800, 0);      /* a big real refresh */
        double before = f.windN; fb_atmo_slew(&f, 0.01);
        ck(f.windN - before < 0.01, "one 100 Hz tick moves the wind <1 cm/s (no airflow step)");
    }

    tsection("atmosphere: Dryden gusts");
    {
        /* calm air: sigma 0 means no vertical gust and no buffet, however long we run */
        fb_atmo a; fb_atmo_init(&a);
        fb_atmo_set_target(&a, 0, 0, 0.0, 0.0, 800, 0); fb_atmo_snap(&a);
        for(int i = 0; i < 2000; i++) fb_atmo_step_gusts(&a, 0.01, 200, 20);
        ck(a.wg == 0.0 && a.gustP == 0.0 && a.gustQ == 0.0, "sigma=0, turb=0 -> dead calm");
        ck(a.gustN == 0.0 && a.gustE == 0.0, "...and no horizontal wander either");

        /* rough air: the vertical gust RMS should land near the commanded sigma */
        fb_atmo b; fb_atmo_init(&b);
        fb_atmo_set_target(&b, 0, 0, 1.0, 1.0, 800, 0); fb_atmo_snap(&b);
        double ss = 0; int n = 0;
        for(int i = 0; i < 200000; i++){
            fb_atmo_step_gusts(&b, 0.01, 500, 20);      /* high AGL: the near-ground ease is inactive */
            if(i > 5000){ ss += b.wg*b.wg; n++; }        /* let the filter settle first */
        }
        double rms = sqrt(ss/n);
        ck(rms > 0.4 && rms < 1.8, "vertical gust RMS is the right order for sigma=1");
        ck(fabs(b.gustP) < 40.0 && fabs(b.gustQ) < 40.0, "buffet rates stay physically sane");

        /* near the ground the gusts are eased so a climb-out isn't thrown around */
        fb_atmo lo, hi; fb_atmo_init(&lo); fb_atmo_init(&hi);
        fb_atmo_set_target(&lo, 0,0, 1.0, 1.0, 800, 0); fb_atmo_snap(&lo);
        fb_atmo_set_target(&hi, 0,0, 1.0, 1.0, 800, 0); fb_atmo_snap(&hi);
        double slo = 0, shi = 0; n = 0;
        for(int i = 0; i < 100000; i++){
            fb_atmo_step_gusts(&lo, 0.01, 2,   20);     /* 2 m AGL */
            fb_atmo_step_gusts(&hi, 0.01, 500, 20);     /* 500 m AGL */
            if(i > 5000){ slo += lo.wg*lo.wg; shi += hi.wg*hi.wg; n++; }
        }
        ck(sqrt(slo/n) < sqrt(shi/n), "gusts are eased near the ground (climb-out stays flyable)");

        /* turbulence drives the horizontal wander; it must mean-revert, not random-walk away */
        fb_atmo c; fb_atmo_init(&c);
        fb_atmo_set_target(&c, 0, 0, 1.0, 0.6, 800, 0); fb_atmo_snap(&c);
        double maxg = 0;
        for(int i = 0; i < 200000; i++){
            fb_atmo_step_gusts(&c, 0.01, 300, 20);
            double g = hypot(c.gustN, c.gustE); if(g > maxg) maxg = g;
        }
        ck(maxg > 0.1, "turbulence actually produces horizontal wander");
        ck(maxg < 15.0, "the OU wander mean-reverts (it does not random-walk to infinity)");

        /* speed enters the scale lengths: guard the fmax(speed,3) floor against a div-by-zero */
        fb_atmo d; fb_atmo_init(&d);
        fb_atmo_set_target(&d, 0, 0, 1.0, 1.0, 800, 0); fb_atmo_snap(&d);
        for(int i = 0; i < 500; i++) fb_atmo_step_gusts(&d, 0.01, 0.0, 0.0);   /* parked, on the ground */
        ck(isfinite(d.wg) && isfinite(d.gustP) && isfinite(d.gustN),
           "zero speed at zero AGL stays finite (no div-by-zero in the scale lengths)");
    }
}
