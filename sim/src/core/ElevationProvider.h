/* The ONE seam every core consumer of ground height goes through, so "where is the ground" is an
 * INJECTED dependency and not a hard fb-tiles wire. Synchronous from the caller's POV: an async client
 * polls until the sentinel stops coming back. */
#ifndef ELEVATIONPROVIDER_H
#define ELEVATIONPROVIDER_H

namespace outshine {

/* Matches fb_stream_ground's convention, so TilesElevation is a pure pass-through. The two
 * data-local providers never return it. */
constexpr double kFBElevationUnresolved = -1e9;

/* The one "is this sample usable" test — a named predicate instead of `> -1e8` in three call sites. */
inline bool ElevationResolved(double m) { return m > -1e8; }

class ElevationProvider {
public:
  virtual ~ElevationProvider() = default;

  /* m ASL, or the sentinel if not resolved yet. */
  virtual double GroundElevM(double latDeg, double lonDeg) const = 0;

  /* Area sample, row-major, row 0 = south edge / col 0 = west edge. The default loops GroundElevM,
   * which is correct for every provider; override once a real batch decode is worth the code. False
   * iff `out` is null or the grid is degenerate — a per-point unresolved sample writes the sentinel
   * rather than failing the whole patch. */
  virtual bool GroundElevPatch(double latMinDeg, double lonMinDeg, double latMaxDeg, double lonMaxDeg,
                               int cols, int rows, double *out) const {
    if (!out || cols < 2 || rows < 2) return false;
    for (int r = 0; r < rows; r++) {
      double lat = latMinDeg + (latMaxDeg - latMinDeg) * (double)r / (double)(rows - 1);
      for (int c = 0; c < cols; c++) {
        double lon = lonMinDeg + (lonMaxDeg - lonMinDeg) * (double)c / (double)(cols - 1);
        out[r * cols + c] = GroundElevM(lat, lon);
      }
    }
    return true;
  }
};

} // namespace outshine
#endif
