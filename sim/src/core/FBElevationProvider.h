/* FlightBox — FBElevationProvider: the ONE seam every core consumer of ground height (mission ground-
 * spawn, AGL/radar-alt, crash detection) goes through, so "where is the ground" is an injected
 * dependency rather than a hard fb-tiles wire. Three implementations live behind it: FBConstantElevation
 * (this header, the gym default — the mission's own runway elevation, no data needed), FBTilesElevation
 * (world/, native+WASM — the live fb-tiles DEM), FBBakedDemElevation (core/, the eingebackene
 * Switzerland island — gym's --elev swiss). All three are synchronous from the caller's POV: a client
 * that itself is async (WASM) polls GroundElevM until it stops returning the "not yet resolved"
 * sentinel, exactly as fb_stream_ground's callers already do. */
#ifndef FBELEVATIONPROVIDER_H
#define FBELEVATIONPROVIDER_H

namespace FlightBox {

/* "not yet resolved" sentinel — matches fb_stream_ground's existing convention (FBTerrainLoader.h) so
 * FBTilesElevation is a pure pass-through. FBConstantElevation/FBBakedDemElevation never return it:
 * both are unconditionally available the instant they're constructed. */
constexpr double kFBElevationUnresolved = -1e9;

class FBElevationProvider {
public:
  virtual ~FBElevationProvider() = default;

  /* Ground elevation (m ASL) under (latDeg, lonDeg), or <= kFBElevationUnresolved/1e8 if not resolved
   * yet (see banner). */
  virtual double GroundElevM(double latDeg, double lonDeg) const = 0;

  /* Area sample for future terrain-aware guidance: fills `out` (row-major, `cols`*`rows`, row 0 = south
   * edge, col 0 = west edge) with GroundElevM at each grid point over [latMinDeg,latMaxDeg] x
   * [lonMinDeg,lonMaxDeg]. Default implementation just loops GroundElevM — correct for every provider;
   * an implementation may override it once a real batch path (e.g. one DEM-tile decode covering the
   * whole patch) is worth the code. Returns false iff `out` is null or the grid is degenerate
   * (cols<2 or rows<2); a per-point unresolved sample still writes the sentinel, it does not fail
   * the whole patch. */
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

} // namespace FlightBox
#endif
