/* xpjsb/flightlog — see flightlog.h. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "constants.h"
#include "flightlog.h"
#include "actuators.h"
#include "../bridge/msp.h"
#include "../sim_state.h"

void xpjsb_flightlog(long tick, double dt){
    static long ivl = 0;
    if(!ivl){ const char *e=getenv("FLT_LOG_S"); double s=e?atof(e):XPJSB_FLT_LOG_S_DEFAULT;
              ivl=(long)(s/dt); if(ivl<1) ivl=1; }
    if(tick % ivl) return;

    double hdg = S.yaw<0 ? S.yaw+360 : S.yaw;
    /* nav state names (MSP_NAV_STATUS): 0 none,1 rth,2 poshold,3 wp,4 proc-land,5 wp-in-progress… — print raw+mode */
    fprintf(stderr,
        "[flt] t=%lds %.5f,%.5f alt%.0fg/%.0fa hdg%03.0f pit%+.1f rol%+.1f as%.1f gs%.1f vs%+.1f "
        "| iNav navst=%d wp=%d home d%d dir%d | modes=0x%x thr%.2f fix%d/%d bad%ld\n",
        tick/(long)lround(1.0/dt), S.lat, S.lon, S.agl, S.elev, hdg, S.pitch, S.roll,
        S.speed, S.gs, S.vy, t_navstate, t_navwp, t_inav_dth, t_inav_dir,
        t_modeflags, S.in_thr, t_fix, t_sats, xpjsb_actuators_bad_count());
}
