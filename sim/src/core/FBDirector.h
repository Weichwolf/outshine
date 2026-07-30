/* The vocabulary of a DIRECTOR delivery — the unguided air-to-ground procedure in which the pilot
 * CONSENTS and the aircraft RELEASES, as against a release cue on which the pilot presses and the
 * press IS the release. One enum for the state, one for why a consent was refused; the mechanism and
 * its sources are doc/modules/mig29/weapons.md §5.4.
 * Core because it is BUS vocabulary: FBFireControlBlock carries both ordinals. */
#ifndef FBDIRECTOR_H
#define FBDIRECTOR_H

#include <cstdint>

namespace FlightBox {

/* Append only — the ordinal is a bus field and an events.log value.
 *   Off      no unguided store selected: the air-to-ground branch is not engaged at all
 *   Search   engaged, nothing ranged — the pilot flies the aiming mark onto the target
 *   Ranged   a slant range exists and is AGEING (the documented 1..10 s window runs from here)
 *   Steer    consent held, a trajectory planned, a release time committed to
 *   Release  the countdown reached zero this tick: the AIRCRAFT lets go
 *   Spent    the store is gone; the pass is over
 *   Refused  a consent was rejected, with a Reason */
enum class FBDirectorState : uint8_t { Off = 0, Search, Ranged, Steer, Release, Spent, Refused };

inline const char *FBDirectorStateStr(FBDirectorState s) {
  switch (s) {
    case FBDirectorState::Search: return "search";
    case FBDirectorState::Ranged: return "ranged";
    case FBDirectorState::Steer: return "steer";
    case FBDirectorState::Release: return "release";
    case FBDirectorState::Spent: return "spent";
    case FBDirectorState::Refused: return "refused";
    case FBDirectorState::Off: break;
  }
  return "off";
}

/* Why a consent did not become a countdown. Every value is a documented boundary of the procedure,
 * never a FlightBox rule — doc/modules/mig29/weapons.md §5.4.2. */
enum class FBDirectorRefusal : uint8_t {
  None = 0,
  NotEngaged,      /* no unguided store selected */
  NoRange,         /* nothing ranged, or the aim point is beyond the device's reach */
  RangeTooFresh,   /* consent inside the documented 1 s minimum */
  RangeStale,      /* consent past the documented 10 s maximum */
  NoReleasePoint,  /* the plan found no release point AHEAD: DCS-EA p.101 gives no procedure */
  WouldNotArm,     /* the planned release leaves the fuze less than its arming delay */
};

inline const char *FBDirectorRefusalStr(FBDirectorRefusal r) {
  switch (r) {
    case FBDirectorRefusal::NotEngaged: return "not_engaged";
    case FBDirectorRefusal::NoRange: return "no_range";
    case FBDirectorRefusal::RangeTooFresh: return "range_too_fresh";
    case FBDirectorRefusal::RangeStale: return "range_stale";
    case FBDirectorRefusal::NoReleasePoint: return "no_release_point";
    case FBDirectorRefusal::WouldNotArm: return "would_not_arm";
    case FBDirectorRefusal::None: break;
  }
  return "none";
}

} // namespace FlightBox
#endif
