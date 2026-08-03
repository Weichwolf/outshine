/* FlightBox — FBGroundMap: the air-to-ground half of a fire-control radar. It turns terrain height
 * into what the antenna actually gets back — grazing angle per resolution cell plus geometric
 * shadowing behind every crest — and publishes that raster into FBGroundMapBlock. Owned by
 * sensors/FBRadarSystem, because it IS the same set in another mode, not a second box.
 * Model, constants and their derivation: doc/sensors.md. */
#ifndef FBGROUNDMAP_H
#define FBGROUNDMAP_H

#include <vector>

#include "FBAvionicsBlocks.h"
#include "FBElevationProvider.h"

namespace FlightBox::Sensors {

class FBGroundMap {
public:
  /* THE CLUTTER MODEL, and the whole of it: constant-gamma, sigma0 = gamma * sin(psi), psi = the LOCAL
   * grazing angle between the beam and the terrain facet. The published cell is sigma0 divided by its
   * own maximum, i.e. sin(psi) itself — normalised by a bound and not by an invented reference angle,
   * so there is no free constant in the picture at all. gamma (the terrain's own reflectivity) cancels
   * with it, which is why a GM map may not be read as an absolute return level.
   * NO RANGE TERM, on purpose: clutter power per cell goes as sigma0 * A_cell / R^4 with the cell area
   * A_cell ~ R * dAz * dR, i.e. as R^-3 — and a set's STC is exactly the amplifier ramp that takes that
   * back out. That cancellation is why a real map is readable from the apex to the outer arc. */

  /* Posts across the cached terrain patch, per axis. Chosen against the coarsest cell the raster can
   * ask for: at the 40 nm scale a range bin is 74 km/48 = 1543 m, and 192 posts over 2.3 x 74 km give
   * 886 m — finer than a cell, so a crest is never missed between two posts of one cell. */
  static constexpr int kPatchN = 192;
  /* The patch reaches this much further than the mapped range, so the aircraft may fly on for a while
   * before the whole square has to be refetched. */
  static constexpr double kPatchMargin = 1.15;
  /* Sub-samples per range bin: a resolution cell returns the SUM of its scatterers, not one probe. */
  static constexpr int kSubPerBin = 4;

  /* Borrowed, never owned; null = this set cannot map (every unit before one is wired). */
  void SetTerrain(const FBElevationProvider *terrain) { Terrain_ = terrain; }
  bool CanMap() const { return Terrain_ != nullptr; }

  /* One antenna cycle. `frameS` is the mapping mode's own sweep time, so the picture paints at the
   * rate the set really scans and not at whatever cadence the module happens to call this. */
  void Run(FBGroundMapBlock &out, double latDeg, double lonDeg, double altAslM, double yawDeg,
           double azHalfDeg, double rangeM, double frameS, double nowS);

  /* The set left the mapping mode: the picture stops being a reading. */
  void Stop(FBGroundMapBlock &out);

private:
  bool BuildPatch(double latDeg, double lonDeg, double halfM);
  /* Height at a planar offset from the patch centre, bilinear; kFBElevationUnresolved outside it. */
  double HeightAt(double eastM, double northM) const;
  void PaintColumn(FBGroundMapBlock &out, int col, double latDeg, double lonDeg, double altAslM,
                   double yawDeg, double azHalfDeg, double rangeM);

  const FBElevationProvider *Terrain_ = nullptr;
  std::vector<double> Patch_;          /* row-major, row 0 = south, col 0 = west; empty until mapped */
  double PatchLat_ = 0.0, PatchLon_ = 0.0, PatchHalfM_ = 0.0, PatchCosLat_ = 1.0;
  bool PatchOk_ = false;
  double LastS_ = -1.0;
  double Phase_ = 0.0;                 /* 0..1 sweep position; the painted column follows it */
  int PaintedTo_ = 0;                  /* columns of the CURRENT sweep already written */
};

} // namespace FlightBox::Sensors
#endif
