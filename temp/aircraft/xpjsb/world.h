/* xpjsb/world — the JSBSim World: owns the flight-state globals, spawns the aircraft on the exact DEM
 * ground, and steps the 6-DOF. Single responsibility: physics. Knows nothing of iNav's protocol. */
#ifndef XPJSB_WORLD_H
#define XPJSB_WORLD_H

extern int g_crashed;   /* set the instant the aircraft is below ground while flying — never hidden */

/* Env-driven: AIRCRAFT, MODELS_ROOT, ORIGIN_LAT/LON, SPAWN_SPEED, FBW, WIND_SPEED/DIR/TURB, TILES_ADDR.
 * Blocks for the exact fb-tiles ground elevation (a guessed altitude spawns under terrain). Returns 0. */
int  world_init(void);

/* One physics step. `armed` (from iNav's MSP telemetry) gates it: a hand-held/parked model is HELD
 * level and still until iNav arms (an untrimmed airframe departs open-loop before iNav could catch it);
 * once armed, JSBSim runs from its trimmed IC and iNav flies it. Writes S, g_nx/ny/nz, S.agl, g_crashed. */
void world_step(double dt, int armed);

#endif /* XPJSB_WORLD_H */
