/* xpjsb/flightlog — one pilot's-eye line every FLT_LOG_S sim-seconds: everything needed to read the
 * flight at a glance. The target/nav readout comes from iNav's OWN telemetry (MSP), because the CC —
 * not the bridge — owns the mission; the bridge just reports what the pilot (iNav) is doing. */
#ifndef XPJSB_FLIGHTLOG_H
#define XPJSB_FLIGHTLOG_H

void xpjsb_flightlog(long tick, double dt);   /* gated internally on the FLT_LOG_S interval */

#endif /* XPJSB_FLIGHTLOG_H */
