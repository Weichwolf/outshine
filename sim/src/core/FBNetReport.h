/* What ONE NODE of an air-defence net puts on the air, and the whole boundary in one struct: a POINT
 * its own radar measured, how old that measurement was when it was sent, and the weapons control state
 * the node transmits. There is no id field and no team field — not as economy but because the report is
 * built from an FBRadarContact, which has neither. A battery therefore cannot learn "hostile" from the
 * net; it can learn only WHERE TO LOOK. doc/air-defence-network.md §§1/11.
 *
 * It rides FBUnitSignature (published at the pose barrier) and arrives as FBDatalinkTrack::Net, so it
 * carries the SENDER's identity and age exactly as every cooperative message does — and says nothing
 * about the target beyond a place and a staleness. */
#ifndef FBNETREPORT_H
#define FBNETREPORT_H

#include <cstdint>
#include <cstring>

namespace FlightBox {

/* Ordinals are telemetry-visible — append, never reorder. */
enum class FBWeaponsControl : uint8_t { Free = 0, Tight, Hold };

inline const char *FBWeaponsControlStr(FBWeaponsControl w) {
  switch (w) {
    case FBWeaponsControl::Free: return "free";
    case FBWeaponsControl::Tight: return "tight";
    case FBWeaponsControl::Hold: return "hold";
  }
  return "?";
}

inline bool FBWeaponsControlFromString(const char *s, FBWeaponsControl &out) {
  if (!s) return false;
  if (std::strcmp(s, "free") == 0) { out = FBWeaponsControl::Free; return true; }
  if (std::strcmp(s, "tight") == 0) { out = FBWeaponsControl::Tight; return true; }
  if (std::strcmp(s, "hold") == 0) { out = FBWeaponsControl::Hold; return true; }
  return false;
}

struct FBNetReport {
  bool   Reporting = false;               /* false = this unit is on no net, or has nothing to report */
  double LatDeg = 0.0, LonDeg = 0.0;      /* the reported POINT, reconstructed from the node's own echo */
  float  AltM = 0.0f;
  /* THE FIRST OF THE TWO STALENESS TERMS: the node's own radar's look age at the moment it reported.
   * The second is FBDatalinkTrack::AgeS, computed by the receiver. Both are published, so a mission can
   * read which half hurt. */
  float  TgtLookAgeS = 0.0f;
  FBWeaponsControl Wcs = FBWeaponsControl::Free;
};

} // namespace FlightBox
#endif
