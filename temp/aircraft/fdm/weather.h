/* FlightBox FDM — live weather ingest (Open-Meteo).
 * Background thread; feeds the atmosphere targets and the cloud/visibility telemetry.
 * Never blocks the FDM. */
#ifndef FB_WEATHER_H
#define FB_WEATHER_H

extern int   g_wx_live;          /* 0 = stay on env/default weather, no network */
extern float g_cloud, g_vis;     /* live cloud cover 0..1, horizontal visibility (m) */

void *wx_thread(void *arg);       /* pthread entry: refresh every 15 min */

#endif /* FB_WEATHER_H */
