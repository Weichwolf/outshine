/* FlightBox tiles — ground elevation service.
 *
 * Answers one question for the flight model: "how high is the ground at this lat/lon?"
 * It fetches the Terrarium DEM tile from upstream, caches it (memory + disk), decodes it with
 * the vendored osmmesh terrain decoder and bilinearly samples it.
 *
 * This exists because the FDM used to hardcode HOME_ELEV=71 m — it believed the world was a
 * flat disc, so AGL was wrong everywhere except exactly over home, while the renderer drew
 * real terrain. Aircraft and world disagreed about the ground.
 */
#ifndef FB_ELEV_H
#define FB_ELEV_H

/* Prepare the cache directory. Returns 0 on success. */
int  fb_elev_init(const char *cache_dir);

/* Ground elevation in metres above sea level at lat/lon.
 * Returns 1 and writes *out on success; 0 if the tile could not be obtained (caller should
 * keep its previous value rather than pretend the ground moved). Blocking: call it off the
 * real-time path. */
int  fb_elev_at(double lat, double lon, double *out);

/* Cache statistics, for the /health endpoint and for spotting upstream trouble. */
void fb_elev_stats(long *hits, long *misses, long *fetch_fail);

#endif /* FB_ELEV_H */
