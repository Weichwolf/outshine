/* FlightBox — outer nav loop (companion computer): senderless auto-arm + autonomous
 * mission (climb / vector-field loiter / RTH), pushed to iNav as RC over MSP. */
#ifndef FB_AUTOPILOT_H
#define FB_AUTOPILOT_H
#include <time.h>

extern double g_loalt, g_lorad;   /* loiter altitude (m AGL) + orbit radius (m), env-tunable */

/* One autopilot tick: arm sequence, mission/manual/RTH mode select, RC to iNav.
 * Gated internally on msp_fd and tick%2. t0 = process start (elapsed timing). */
void autopilot_step(long tick, struct timespec t0, double dt,
                    float cr, float cp, float cy, float cthr, int link_up);

#endif /* FB_AUTOPILOT_H */
