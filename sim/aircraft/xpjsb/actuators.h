/* xpjsb/actuators — iNav's control outputs (X-Plane yoke/throttle datarefs) -> JSBSim controls.
 * Single responsibility: validate the wire values and hold the last good command. */
#ifndef XPJSB_ACTUATORS_H
#define XPJSB_ACTUATORS_H

/* Feed one control dataref (name,value) from an inbound DREF packet. Out-of-range values (the
 * infamous -3.0 yoke glitch) are rejected; the last good command is held. Returns 1 if it was a
 * recognized (and accepted) control, 0 otherwise. */
int  xpjsb_actuator_dref(const char *dref, float value);

/* Push the current held command into JSBSim (throttle slewed). Call once per world step. */
void xpjsb_actuators_apply(double dt);

/* Count of rejected out-of-range control samples (diagnostic). */
long xpjsb_actuators_bad_count(void);

#endif /* XPJSB_ACTUATORS_H */
