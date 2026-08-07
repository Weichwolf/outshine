#ifndef GRATICULE_H
#define GRATICULE_H

#include "Units.h"

#include <cmath>

namespace outshine::Render {

/* THE LATTICE THE STAND IS HASHED ON, and it is the graticule: a cell index is the floor of
 * degrees-of-arc / kCellM, so it is a function of the PLACE and of nothing else — not of where the
 * camera stands, not of where the session started. A stand whose lattice slid with the viewer would
 * change species under a fixed point as the camera walked; measured on the reference scene at 1.7 m
 * eye, an eye-centred lattice swung the coverage of a FIXED world strip 0.026 -> 0.151 -> 0.026 with
 * a period of exactly its own spacing.
 *
 * The cell is kCellM north by kCellM*cos(lat) east, so it shrinks east with latitude; the shader is
 * handed both edges in metres and never has to know the latitude. */
struct Graticule {
  static constexpr double kCellM = 1.0;
  static constexpr double kMinCosLat = 0.25;   /* cos 75.5 deg; past it the cell stops shrinking */

  double CellE = kCellM;             /* ground metres east of one cell */
  double FracE = 0.0, FracN = 0.0;   /* the eye's offset inside its own cell, ground metres */
  long BaseI = 0, BaseJ = 0;         /* the eye's own cell */

  static Graticule At(double latDeg, double lonDeg) {
    Graticule g;
    const double c = std::cos(latDeg * kDeg2Rad);
    g.CellE = kCellM * (c > kMinCosLat ? c : kMinCosLat);
    const double gx = lonDeg * kMPerDeg / kCellM, gy = latDeg * kMPerDeg / kCellM;
    g.BaseI = (long)std::floor(gx);
    g.BaseJ = (long)std::floor(gy);
    g.FracE = (gx - (double)g.BaseI) * g.CellE;
    g.FracN = (gy - (double)g.BaseJ) * kCellM;
    return g;
  }
};

} // namespace outshine::Render
#endif
