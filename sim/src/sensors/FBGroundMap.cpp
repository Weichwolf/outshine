#include "FBGroundMap.h"

#include <cmath>

#include "FBLog.h"
#include "FBUnits.h"

namespace FlightBox::Sensors {

namespace {
constexpr double kDeg = 3.14159265358979323846 / 180.0;

/* A patch is only a picture, so the planar (equirectangular) reduction FBGeodesy already uses for
 * every range/bearing on this bus is used here too: over the 170 km a 40 nm map spans it costs under
 * 0.1 % in scale, far below one range bin. */
double MPerDegLon(double latDeg) {
  double c = std::cos(latDeg * kDeg);
  return kMPerDeg * (c > 1e-6 ? c : 1e-6);
}
} // namespace

bool FBGroundMap::BuildPatch(double latDeg, double lonDeg, double halfM) {
  if (!Terrain_) return false;
  if (Patch_.size() != (size_t)kPatchN * kPatchN) Patch_.resize((size_t)kPatchN * kPatchN);
  double dLat = halfM / kMPerDeg, dLon = halfM / MPerDegLon(latDeg);
  PatchOk_ = Terrain_->GroundElevPatch(latDeg - dLat, lonDeg - dLon, latDeg + dLat, lonDeg + dLon,
                                       kPatchN, kPatchN, Patch_.data());
  PatchLat_ = latDeg;
  PatchLon_ = lonDeg;
  PatchHalfM_ = halfM;
  PatchCosLat_ = MPerDegLon(latDeg) / kMPerDeg;
  if (!PatchOk_) return false;
  /* A patch that is mostly sentinel is a provider that has not answered yet, not flat ground — and a
   * map drawn over it would be a confident black square. */
  int unresolved = 0;
  for (double h : Patch_)
    if (!FBElevationResolved(h)) unresolved++;
  PatchOk_ = unresolved * 4 < (int)Patch_.size();
  return PatchOk_;
}

double FBGroundMap::HeightAt(double eastM, double northM) const {
  if (!PatchOk_) return kFBElevationUnresolved;
  double u = (eastM / (PatchHalfM_ * 2.0) + 0.5) * (kPatchN - 1);
  double v = (northM / (PatchHalfM_ * 2.0) + 0.5) * (kPatchN - 1);
  if (u < 0.0 || v < 0.0 || u > kPatchN - 1 || v > kPatchN - 1) return kFBElevationUnresolved;
  int c0 = (int)u, r0 = (int)v;
  if (c0 > kPatchN - 2) c0 = kPatchN - 2;
  if (r0 > kPatchN - 2) r0 = kPatchN - 2;
  double fu = u - c0, fv = v - r0;
  const double *p = Patch_.data();
  double h00 = p[(size_t)r0 * kPatchN + c0], h10 = p[(size_t)r0 * kPatchN + c0 + 1];
  double h01 = p[(size_t)(r0 + 1) * kPatchN + c0], h11 = p[(size_t)(r0 + 1) * kPatchN + c0 + 1];
  if (!FBElevationResolved(h00) || !FBElevationResolved(h10) || !FBElevationResolved(h01) ||
      !FBElevationResolved(h11))
    return kFBElevationUnresolved;
  return (h00 * (1 - fu) + h10 * fu) * (1 - fv) + (h01 * (1 - fu) + h11 * fu) * fv;
}

/* ONE beam position, marched outward in range. Two things happen along the ray and both are geometry:
 * the running MINIMUM depression angle decides what the beam can still see (everything behind a crest
 * is looked over, not through), and the along-ray height gradient turns the depression angle into the
 * local grazing angle the constant-gamma clutter model wants. */
void FBGroundMap::PaintColumn(FBGroundMapBlock &out, int col, double latDeg, double lonDeg,
                              double altAslM, double yawDeg, double azHalfDeg, double rangeM) {
  double azDeg = -azHalfDeg + (col + 0.5) * (2.0 * azHalfDeg / kGroundMapAz);
  double brg = (yawDeg + azDeg) * kDeg;
  double sinB = std::sin(brg), cosB = std::cos(brg);
  double ds = rangeM / (double)(kGroundMapRange * kSubPerBin);
  double eastM = (lonDeg - PatchLon_) * kMPerDeg * PatchCosLat_;
  double northM = (latDeg - PatchLat_) * kMPerDeg;

  double deltaMin = 1.5707963;   /* nothing looked at yet */
  double hPrev = HeightAt(eastM, northM);
  bool havePrev = FBElevationResolved(hPrev);
  for (int k = 0; k < kGroundMapRange; k++) {
    double acc = 0.0;
    for (int j = 0; j < kSubPerBin; j++) {
      double s = (double)(k * kSubPerBin + j + 1) * ds;
      double h = HeightAt(eastM + s * sinB, northM + s * cosB);
      if (!FBElevationResolved(h)) { havePrev = false; continue; }
      double delta = std::atan2(altAslM - h, s);
      if (delta >= deltaMin) { hPrev = h; havePrev = true; continue; }   /* looked over, no echo */
      deltaMin = delta;
      double g = havePrev ? (h - hPrev) / ds : 0.0;
      hPrev = h;
      havePrev = true;
      double sinPsi = (std::sin(delta) + std::cos(delta) * g) / std::sqrt(1.0 + g * g);
      if (sinPsi > 0.0) acc += sinPsi;   /* <= 0 = the facet is turned away from the beam: no echo */
    }
    double cell = acc / (double)kSubPerBin;
    out.Cell[k][col] = (uint8_t)(cell * 255.0 + 0.5);
  }
}

void FBGroundMap::Run(FBGroundMapBlock &out, double latDeg, double lonDeg, double altAslM,
                      double yawDeg, double azHalfDeg, double rangeM, double frameS, double nowS) {
  if (!Terrain_ || rangeM <= 0.0 || azHalfDeg <= 0.0) { Stop(out); return; }
  double halfM = rangeM * kPatchMargin;
  double driftE = (lonDeg - PatchLon_) * kMPerDeg * PatchCosLat_, driftN = (latDeg - PatchLat_) * kMPerDeg;
  double drift = std::sqrt(driftE * driftE + driftN * driftN);
  if (!PatchOk_ || std::fabs(halfM - PatchHalfM_) > 1.0 || drift > halfM - rangeM) {
    if (!BuildPatch(latDeg, lonDeg, halfM)) {
      out.Mapping = false;
      out.H.Invalidate();
      return;
    }
    PaintedTo_ = 0;   /* the old picture was drawn against a different square */
    Phase_ = 0.0;
  }

  double dt = (LastS_ < 0.0 || nowS < LastS_) ? 0.0 : nowS - LastS_;
  LastS_ = nowS;
  double sweepS = frameS > 0.05 ? frameS : 0.05;
  double step = dt / sweepS;
  if (step > 1.0) step = 1.0;   /* a long step paints ONE full sweep, never a queue of them */
  Phase_ += step;
  /* ONE line per completed sweep, so the picture is checkable as NUMBERS and not only as pixels:
   * mean echo over the raster and the share of cells that got any echo at all (the rest is shadow or
   * a facet turned away), against which an independent terrain walk can be held. */
  if (Phase_ >= 1.0) {
    Phase_ -= 1.0;
    PaintedTo_ = 0;
    long sum = 0, lit = 0;
    for (int k = 0; k < kGroundMapRange; k++)
      for (int c = 0; c < kGroundMapAz; c++) { sum += out.Cell[k][c]; lit += out.Cell[k][c] > 0; }
    const double cells = (double)kGroundMapRange * kGroundMapAz;
    FBLog::Debug("radar", "groundmap",
                 {{"azHalfDeg", azHalfDeg}, {"rangeNm", rangeM * kMToNm}, {"altAslM", altAslM},
                  {"meanEcho", (double)sum / cells / 255.0}, {"litFrac", (double)lit / cells},
                  {"patchPosts", kPatchN * kPatchN}});
  }
  int target = (int)(Phase_ * kGroundMapAz) + 1;
  if (target > kGroundMapAz) target = kGroundMapAz;
  /* Only the columns the beam crossed since the last call are rewritten — what it has not revisited
   * keeps the previous sweep's paint, which is what makes a radar map a picture and not a flicker. */
  for (int c = PaintedTo_; c < target; c++)
    PaintColumn(out, c, latDeg, lonDeg, altAslM, yawDeg, azHalfDeg, rangeM);
  if (target > PaintedTo_) PaintedTo_ = target;

  out.Mapping = true;
  out.AzHalfDeg = (float)azHalfDeg;
  out.RangeM = (float)rangeM;
  out.SweepFrac = (float)Phase_;
  double g0 = HeightAt((lonDeg - PatchLon_) * kMPerDeg * PatchCosLat_, (latDeg - PatchLat_) * kMPerDeg);
  out.GroundAslM = FBElevationResolved(g0) ? (float)g0 : 0.0f;
  out.H.Publish(nowS);
}

void FBGroundMap::Stop(FBGroundMapBlock &out) {
  out.Mapping = false;
  out.H.Invalidate();
  Phase_ = 0.0;
  PaintedTo_ = 0;
  LastS_ = -1.0;
}

} // namespace FlightBox::Sensors
