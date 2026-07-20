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

/* Apply iNav's AUXILIARY servo outputs (read over MSP_SERVO) to the FDM per the model's AUXMAP env
 * ("gear:<idx>,flap:<idx>,speedbrake:<idx>", servo index into MSP_SERVO). Gear polarity is inverted so
 * an un-commanded channel = gear DOWN. The 3 primary surfaces stay on the fast --sim=xp path. */
void xpjsb_aux_apply(void);

/* Count of rejected out-of-range control samples (diagnostic). */
long xpjsb_actuators_bad_count(void);

#endif /* XPJSB_ACTUATORS_H */
