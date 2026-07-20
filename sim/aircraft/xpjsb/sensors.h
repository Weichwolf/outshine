/* xpjsb/sensors — maps the JSBSim world state to the X-Plane dataref values iNav (--sim=xp) reads.
 * Single responsibility: sensor synthesis. Owns the --useimu magnetometer (dipped geomagnetic field). */
#ifndef XPJSB_SENSORS_H
#define XPJSB_SENSORS_H

/* Value for a subscribed X-Plane dataref string, synthesized from the current world state (sim_state.h
 * S + the body load factors g_nx/g_ny/g_nz). Unknown datarefs return 0. */
float xpjsb_sensor_value(const char *dref);

#endif /* XPJSB_SENSORS_H */
