/* FlightBox — FBF16Rwr: the AN/ALR-56M, an override of systems/FBRwrSystem carrying only the antenna
 * coverage and the display cap, both [DOC] defence-rwr-cm.md §2.1. The threat LIBRARY is deliberately
 * absent: the source describes its structure without transcribing it, and inventing symbol codes would
 * be inventing the one thing an RWR may not guess about. Details: doc/flightbox/modules-f16.md §6. */
#ifndef FBF16RWR_H
#define FBF16RWR_H

#include "FBRwrSystem.h"

namespace FlightBox {

/* Ordinals are telemetry-visible — append, never reorder. */
enum class FBF16RwrDisplay { Priority, Open };

inline bool FBF16RwrDisplayFromString(const char *s, FBF16RwrDisplay &out) {
  if (!std::strcmp(s, "priority")) { out = FBF16RwrDisplay::Priority; return true; }
  if (!std::strcmp(s, "open")) { out = FBF16RwrDisplay::Open; return true; }
  return false;
}

class FBF16Rwr : public FBRwrSystem {
public:
  /* 360 deg azimuth but only +-45 in ELEVATION — a genuine blind spot above and below the fuselage
   * that the jet's own defensive manoeuvring rotates a hostile radar into, silently dropping the
   * warning. The outcome-relevant number of this class. */
  static constexpr double kElevCoverageDeg = 45.0;
  /* DISPLAY caps, not detection limits: the table below keeps ranking everything it hears. OPEN's 16
   * exceeds the table's own size deliberately, so the cap stops binding the moment it grows. */
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
