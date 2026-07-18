/* FlightBox aircraft — ground elevation client.
 *
 * The FDM needs to know the height of the ground beneath the aircraft, otherwise it cannot
 * know its true AGL. It used to hardcode HOME_ELEV=71 m: the world was a flat disc, so AGL was
 * wrong everywhere except exactly over home, while the renderer drew real terrain — aircraft
 * and world disagreed about the ground.
 *
 * This asks the fb-tiles world-data service instead. Design constraints:
 *   - The FDM runs at 100 Hz and must NEVER block on I/O, so a background thread polls and the
 *     hot path only reads a cached value.
 *   - Ground elevation changes slowly relative to flight (at 18 m/s a 250 ms poll is ~4.5 m of
 *     travel), so polling is entirely adequate — no need for a local DEM in the aircraft.
 *   - If the service is unreachable we KEEP THE LAST GOOD VALUE. Never silently claim the
 *     ground is at zero: that would hand the autopilot a cliff that isn't there.
 */
#ifndef FB_AC_TERRAIN_H
#define FB_AC_TERRAIN_H

/* Start the poller. `addr` is host[:port] of fb-tiles (e.g. "fb-tiles:8081").
 * `seed_elev` is used until the first successful reply. Returns 0 on success. */
int    fb_terrain_start(const char *addr, double seed_elev);

/* Tell the poller where we are. Cheap, non-blocking — call from the FDM loop. */
void   fb_terrain_set_pos(double lat, double lon);

/* Latest known ground elevation (m AMSL) under the last reported position. Non-blocking. */
double fb_terrain_ground(void);

/* One blocking lookup, for start-up (getting home's elevation before the sim runs).
 * Returns 1 and writes *out on success, 0 on failure. */
int    fb_terrain_lookup(const char *addr, double lat, double lon, double *out);

/* Blocking start-up lookup with retry up to deadline_s. Asks fb-tiles to wait for the (possibly
 * cold) DEM tile (?block=1) and rides out the container boot race. Returns 1 and writes *out on
 * success, 0 if the whole deadline elapsed. Use ONLY at start-up — never on the FDM hot path. */
int    fb_terrain_lookup_deadline(const char *addr, double lat, double lon, double *out, double deadline_s);

/* Diagnostics: successful updates and consecutive failures. */
void   fb_terrain_stats(long *ok, long *fail);

#endif /* FB_AC_TERRAIN_H */
