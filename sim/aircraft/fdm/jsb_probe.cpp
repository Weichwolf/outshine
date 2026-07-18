/* FlightBox — JSBSim link/load/tick probe (Migrationspaket D, Stufe D1).
 * Standalone, KEIN iNav, KEIN state_t S: beweist nur, dass libJSBSim linkt, ein Aircraft-Plugin
 * aus sim/aircraft/models/<name>/ lädt (per-Modell aircraft/engine/systems-Pfade) und physikalisch
 * plausibel tickt (finit, kein sofortiges NaN/Explodieren). Der echte Adapter (Props<->S) ist D2. */
#include "FGFDMExec.h"
#include "initialization/FGInitialCondition.h"
#include "input_output/string_utilities.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>

using namespace JSBSim;

int main(int argc, char** argv) {
    const char* model = argc > 1 ? argv[1] : "minisgs_e";
    double vc_kts     = argc > 2 ? atof(argv[2]) : 55.0;   /* minisgs ~28 m/s; f16 ~300 */
    double thr        = argc > 3 ? atof(argv[3]) : 0.6;
    std::string root  = "models";
    std::string mdir  = root + "/" + model;

    FGFDMExec fdm;
    fdm.SetDebugLevel(0);

    if (!fdm.LoadModel(SGPath(root), SGPath(mdir + "/engine"), SGPath(mdir + "/Systems"), model)) {
        printf("PROBE FAIL: LoadModel(%s) returned false\n", model);
        return 1;
    }
    printf("loaded %s\n", model);

    auto ic = fdm.GetIC();
    ic->SetLatitudeDegIC(52.045);
    ic->SetLongitudeDegIC(9.385);
    ic->SetAltitudeASLFtIC(2000.0);       /* ~610 m */
    ic->SetVcalibratedKtsIC(vc_kts);
    ic->SetFlightPathAngleDegIC(0.0);      /* level */
    fdm.RunIC();

    fdm.Setdt(0.01);                       /* 100 Hz, wie der Bridge-Loop */
    fdm.SetPropertyValue("fcs/throttle-cmd-norm", thr);

    /* D2-Vorbereitung: die EXAKTEN Properties dumpen, die der Adapter auf state_t S mappt —
     * bestätigt, dass jeder Name auflöst (kein stiller 0) und der Wert an bekannter IC sane ist. */
    fdm.Run();
    printf("--- property map check (IC: %.0f kts level, %.0f ft) ---\n", vc_kts, 2000.0);
    const char* props[] = {
        "attitude/phi-deg","attitude/theta-deg","attitude/psi-deg",
        "velocities/p-rad_sec","velocities/q-rad_sec","velocities/r-rad_sec",
        "position/lat-geod-deg","position/long-gc-deg","position/h-sl-ft","position/h-agl-ft",
        "velocities/vt-fps","velocities/vg-fps",
        "velocities/v-east-fps","velocities/v-north-fps","velocities/v-down-fps",
        "accelerations/Nz","accelerations/a-pilot-z-ft_sec2", 0 };
    for (int k = 0; props[k]; k++)
        printf("  %-34s = %.5g\n", props[k], fdm.GetPropertyValue(props[k]));
    printf("--- 5 s open-loop tick ---\n");
    printf("  t   phi   theta  psi    alt_ft   vc_kt  vt_fps   p     q     r    thr\n");
    for (int i = 0; i < 500; i++) {        /* 5 s */
        if (!fdm.Run()) { printf("Run() ended/failed at step %d\n", i); return 2; }
        double phi = fdm.GetPropertyValue("attitude/phi-deg");
        double alt = fdm.GetPropertyValue("position/h-sl-ft");
        if (!std::isfinite(phi) || !std::isfinite(alt)) {
            printf("PROBE FAIL: non-finite state at step %d (phi=%.3g alt=%.3g)\n", i, phi, alt);
            return 3;
        }
        if (i % 100 == 0) {
            printf("%5.2f %6.1f %6.1f %6.1f %8.1f %6.1f %7.1f %5.2f %5.2f %5.2f %4.2f\n",
                fdm.GetSimTime(), phi,
                fdm.GetPropertyValue("attitude/theta-deg"),
                fdm.GetPropertyValue("attitude/psi-deg"), alt,
                fdm.GetPropertyValue("velocities/vc-kts"),
                fdm.GetPropertyValue("velocities/vt-fps"),
                fdm.GetPropertyValue("velocities/p-rad_sec"),
                fdm.GetPropertyValue("velocities/q-rad_sec"),
                fdm.GetPropertyValue("velocities/r-rad_sec"),
                fdm.GetPropertyValue("fcs/throttle-pos-norm"));
        }
    }
    printf("PROBE OK: %s loaded, ticked 5 s, state finite\n", model);
    return 0;
}
