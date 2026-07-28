/* FlightBox — FBF16Datalink: the MIDS-LVT terminal, an override of systems/FBDatalinkSystem carrying
 * only the Link-16 reach and the HSD's contact filter. FLIGHT LEAD is a documented stand-in: the
 * simulator has no flight-structure concept to read a lead off, so FL ON keeps the flight's FIRST
 * participant. Details: doc/modules-f16.md §5. */
#ifndef FBF16DATALINK_H
#define FBF16DATALINK_H

#include <cstring>
#include "FBDatalinkSystem.h"

namespace FlightBox::Modules {

/* The HSD's three contact-filter switch positions [DOC]. */
enum class FBF16ContactFilter { FriendlyAll, FlightLeadsOnly, Off };

inline bool FBF16ContactFilterFromString(const char *s, FBF16ContactFilter &out) {
  if (!std::strcmp(s, "fr"))  { out = FBF16ContactFilter::FriendlyAll;     return true; }
  if (!std::strcmp(s, "fl"))  { out = FBF16ContactFilter::FlightLeadsOnly; return true; }
  if (!std::strcmp(s, "off")) { out = FBF16ContactFilter::Off;             return true; }
  return false;
}

class FBF16Datalink : public Sensors::FBDatalinkSystem {
public:
  /* [DOC] Link-16 air-to-air LOS reach; the horizon of the two altitudes usually binds long before it,
   * which is the base class's business. */
  static constexpr double kMidsRangeNm = 300.0;

  FBF16Datalink() { SetMaxRangeM(kMidsRangeNm * 1852.0); }

  void SetContactFilter(FBF16ContactFilter f) { Filter_ = f; }
  FBF16ContactFilter ContactFilter() const { return Filter_; }

protected:
  bool AcceptContact(const Units::FBUnit &sender, int flightIndex) const override {
    (void)sender;
    switch (Filter_) {
      case FBF16ContactFilter::FriendlyAll:     return true;
      case FBF16ContactFilter::FlightLeadsOnly: return flightIndex == 0;
      case FBF16ContactFilter::Off:             return false;
    }
    return false;
  }

private:
  FBF16ContactFilter Filter_ = FBF16ContactFilter::FriendlyAll;   /* HSD default: FR ON */
};

} // namespace FlightBox::Modules
#endif
