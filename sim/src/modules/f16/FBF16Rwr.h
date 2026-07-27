/* FlightBox — FBF16Rwr: the F-16's AN/ALR-56M Radar Warning Receiver, an override of the generic
 * warning receiver (systems/FBRwrSystem — read its banner first; HOW an emission is heard, held and
 * ranked lives there, this file carries only what is ALR-56M about it).
 *
 * WHAT IS ALR-56M ABOUT IT, every item straight out of doc/f16/defence-rwr-cm.md §2.1:
 *   - THE COVERAGE, and it is the one that changes outcomes: four high-band quadrant antennas plus a
 *     dual-blade low-band pair give 360 degrees of azimuth but only +-45 degrees in ELEVATION. The
 *     source spells out the consequence — "a genuine RWR blind spot directly above/below the fuselage
 *     centerline; high-pitch or high-bank defensive maneuvers can rotate a hostile radar into that blind
 *     spot, silently dropping lock/launch warnings". That is not a display quirk, it is a hole in the
 *     aircraft's situational awareness that its own manoeuvring opens, and it is why this number is the
 *     first thing this class states.
 *   - THE DISPLAY CAP: the TWP MODE button switches between PRIORITY (the 5 highest-priority threats)
 *     and OPEN (16). The source is explicit that this is a DISPLAY cap and not a detection limit — more
 *     threats can be detected than are shown — so it is applied to the published list only, over a
 *     detection table that keeps ranking everything it hears.
 *   - THE SEARCH FILTER: the TWA panel's SEARCH button shows or hides early-warning/surveillance/
 *     non-lethal-acquisition symbols, and when they are hidden the panel says so rather than showing
 *     nothing. The generic class already carries both halves; this class only sets the default.
 * The threat LIBRARY (Appendix B's ALIC/symbol/system correlation table) is deliberately not here: the
 * source describes its structure and does not transcribe it, and inventing symbol codes would be
 * inventing the one thing an RWR is not allowed to guess about. Classification therefore stays the
 * generic emitter-class estimate until a real library exists. */
#ifndef FBF16RWR_H
#define FBF16RWR_H

#include "FBRwrSystem.h"

namespace FlightBox {

/* The TWP MODE button's two display modes (doc/f16/defence-rwr-cm.md §2.1). Ordinals are telemetry-
 * visible through `set rwr_mode` — append, never reorder. */
enum class FBF16RwrDisplay { Priority, Open };

inline bool FBF16RwrDisplayFromString(const char *s, FBF16RwrDisplay &out) {
  if (!std::strcmp(s, "priority")) { out = FBF16RwrDisplay::Priority; return true; }
  if (!std::strcmp(s, "open")) { out = FBF16RwrDisplay::Open; return true; }
  return false;
}

class FBF16Rwr : public FBRwrSystem {
public:
  /* The documented antenna geometry (class banner): 360 deg azimuth, +-45 deg elevation. */
  static constexpr double kElevCoverageDeg = 45.0;
  /* The two display caps, verbatim. OPEN's 16 exceeds the detection table's own size
   * (kMaxRwrThreats) — deliberately kept as the documented figure rather than trimmed to it, so the cap
   * stops being the binding limit the moment the table grows. */
  static constexpr int kPriorityThreats = 5;
  static constexpr int kOpenThreats = 16;

  void SetDisplay(FBF16RwrDisplay d) { Display_ = d; }
  FBF16RwrDisplay Display() const { return Display_; }

protected:
  double ElevCoverageDeg() const override { return kElevCoverageDeg; }
  int MaxDisplayed() const override {
    return Display_ == FBF16RwrDisplay::Open ? kOpenThreats : kPriorityThreats;
  }

private:
  FBF16RwrDisplay Display_ = FBF16RwrDisplay::Priority;   /* the jet powers up in PRIORITY */
};

} // namespace FlightBox
#endif
