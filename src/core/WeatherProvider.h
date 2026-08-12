/* ElevationProvider's sibling: the ONE seam every core consumer of ATMOSPHERE goes through, so "what
 * is the air doing here" is an INJECTED dependency and not a hard fb-tiles wire. Same shape as the
 * elevation hook — small interface, data-local default, blob-backed and (browser) live
 * implementations. */
#ifndef WEATHERPROVIDER_H
#define WEATHERPROVIDER_H

namespace outshine {

/* The AIR MASS's own velocity, m/s, north/east/down — JSBSim's FGWinds convention verbatim (see
 * Fdm::SetWindNedMs), NOT the meteorological "wind from" bearing: a wind FROM 270 blows toward the
 * east and is (N=0, E=+v, D=0). */
struct WindNed {
  double N = 0.0, E = 0.0, D = 0.0;
};

/* GFS' four cover diagnostics plus the ceiling, which is the one quantity that can legitimately be
 * ABSENT — a renderer must not draw a cloud base where the model says there is none (§9.6). */
struct CloudLayers {
  double TotalPct = 0.0, LowPct = 0.0, MidPct = 0.0, HighPct = 0.0;
  double CeilingM = 0.0;
  bool   HaveCeiling = false;
};

/* Above the format's own visibility window (24.5 km), so "unlimited" cannot be confused with a measured
 * value inside it. */
constexpr double kFBVisibilityUnlimitedM = 100000.0;

class WeatherProvider {
public:
  virtual ~WeatherProvider() = default;

  /* `altM` is ASL. Interpolated over the pressure levels' own geopotential heights; below the surface
   * level the surface field, above the highest level that level (the blob carries nothing higher). */
  virtual WindNed WindNedMs(double latDeg, double lonDeg, double altM) const = 0;

  virtual CloudLayers Clouds(double latDeg, double lonDeg) const = 0;

  virtual double VisibilityM(double latDeg, double lonDeg) const = 0;
};

} // namespace outshine
#endif
