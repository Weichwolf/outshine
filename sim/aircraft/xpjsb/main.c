/* xpjsb — the XP-JSBSim bridge: presents the JSBSim World to the vanilla iNav SITL over its built-in
 * --sim=xp X-Plane protocol, and applies iNav's actuator outputs back into the physics. Pure sensor/
 * actuator wire — no autopilot, no navigation: iNav (the pilot) flies; the Command Center (ground
 * station) commands it over MSP. Orchestrates the modules; each has a single responsibility.
 *
 * Env: AIRCRAFT, MODELS_ROOT, ORIGIN_LAT/LON, SPAWN_SPEED, FBW, WIND_*, TILES_ADDR, FLT_LOG_S,
 *      XP_LISTEN_PORT (default 49000). FB_TIME_SCALE (via the LD_PRELOAD clock shim) sets sim speed. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "constants.h"
#include "world.h"
#include "xplink.h"
#include "actuators.h"
#include "flightlog.h"
#include "../bridge/msp.h"

#define XPJSB_ARMED_FLAG  (1u << 2)   /* MSP2_INAV_STATUS armingFlags: bit2 = ARMED */

int main(void){
    if(world_init() != 0) return 1;

    int port = getenv("XP_LISTEN_PORT") ? atoi(getenv("XP_LISTEN_PORT")) : XPJSB_XP_PORT;
    int xp = xpjsb_xp_open(port);
    if(xp < 0){ fprintf(stderr,"[xpjsb] FATAL: cannot bind X-Plane UDP port %d\n", port); return 1; }
    fprintf(stderr,"[xpjsb] X-Plane dataref server on :%d, MSP telemetry -> 127.0.0.1:%d\n", port, XPJSB_MSP_PORT);

    const double dt = XPJSB_DT_S;
    long tick = 0;
    for(;;){
        xpjsb_xp_recv(xp);                               /* iNav's control DREFs -> actuators */
        int armed = (t_armflags & XPJSB_ARMED_FLAG) != 0;
        xpjsb_actuators_apply(dt);                       /* held command -> JSBSim inputs */
        world_step(dt, armed);                           /* advance physics (held until armed) */
        xpjsb_xp_send(xp);                               /* sensors -> iNav */

        if(msp_fd < 0 && tick % 50 == 0) msp_fd = msp_connect();  /* telemetry link (diagnostic; the CC owns command) */
        if(msp_fd >= 0){
            msp_poll();
            switch(tick % 20){                           /* round-robin the telemetry reads */
                case 0:  msp2(0x2000); break;            /* arming flags (the armed gate) */
                case 4:  msp1(101,0,0); break;           /* MSP_STATUS -> mode flags */
                case 8:  msp1(121,0,0); break;           /* MSP_NAV_STATUS -> nav state / active wp */
                case 12: msp1(107,0,0); break;           /* MSP_COMP_GPS -> dist/dir to home */
                case 16: msp1(106,0,0); break;           /* MSP_RAW_GPS -> fix/sats */
            }
        }

        xpjsb_flightlog(++tick, dt);
        struct timespec ts = {0, (long)(dt*1e9)}; nanosleep(&ts, NULL);   /* shim scales for fast-sim */
    }
    return 0;
}
