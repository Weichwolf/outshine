/* FlightBox — FBF16Datalink: the F-16's terminal, an override of the generic cooperative net
 * (systems/FBDatalinkSystem — read its banner first; everything about HOW the net works lives there,
 * this file carries only what is F-16 about it).
 *
 * From doc/f16/datalink-iff.md (DCS F-16C Viper Guide, Part 13):
 *   - The radio is the MIDS-LVT (Link-16); the DCS implementation is TNDL. Link-16 is a UHF
 *     line-of-sight, TDMA net whose air-to-air reach is quoted at ~300 nm — the terminal range this
 *     class sets, replacing the base class's deliberately generic placeholder. The LOS horizon of the
 *     two altitudes usually binds long before it (two jets at 2500 m see ~223 nm of each other), which
 *     is the base class's business, not this one's.
 *   - The HSD's datalink CONTACT FILTER is the F-16-specific behaviour: FR ON (all friendly), FL ON
 *     (flight leads only), FR OFF (none). That is precisely FBDatalinkSystem::AcceptContact's job, so
 *     it is the one thing overridden here.
 *
 * FLIGHT LEAD, honestly stated: the simulator has no flight-structure concept yet — no element/section
 * assignment exists to read a lead OFF. FL ON therefore keeps the flight's first participant, which for
 * a mission-declared flight is the first `unit` block of this team (doc/mission-format.md's primary
 * actor). It is a documented stand-in for a real lead assignment, not a model of one. */
#ifndef FBF16DATALINK_H
#define FBF16DATALINK_H

#include <cstring>
#include "FBDatalinkSystem.h"

namespace FlightBox {

/* The HSD's three datalink contact-filter positions (guide, Part 13). */
enum class FBF16ContactFilter { FriendlyAll, FlightLeadsOnly, Off };

/* The `set datalink_filter` mission key's accepted spellings — the switch positions themselves, strict
 * like every other .fbm value (same shape as core/FBTeam.h's FBUnitTeamFromString). */
inline bool FBF16ContactFilterFromString(const char *s, FBF16ContactFilter &out) {
  if (!std::strcmp(s, "fr"))  { out = FBF16ContactFilter::FriendlyAll;     return true; }
  if (!std::strcmp(s, "fl"))  { out = FBF16ContactFilter::FlightLeadsOnly; return true; }
  if (!std::strcmp(s, "off")) { out = FBF16ContactFilter::Off;             return true; }
  return false;
}

class FBF16Datalink : public FBDatalinkSystem {
public:
  /* Link-16 / MIDS-LVT air-to-air line-of-sight reach (see the class banner). */
  static constexpr double kMidsRangeNm = 300.0;

  FBF16Datalink() { SetMaxRangeM(kMidsRangeNm * 1852.0); }

  void SetContactFilter(FBF16ContactFilter f) { Filter_ = f; }
  FBF16ContactFilter ContactFilter() const { return Filter_; }

protected:
  bool AcceptContact(const FBUnit &sender, int flightIndex) const override {
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

} // namespace FlightBox
#endif
