/* FlightBox — FBMig29Sms: WHERE this aircraft's pylons sit, and nothing else — all behaviour is
 * weapons/FBStoresSystem, exactly as modules/f16/FBF16Sms is for the F-16.
 *
 * SIX WING PYLONS PLUS A CENTRELINE STATION is the one certain fact ([DCS-FM p.64], [DCS-EA p.86]); the
 * official NUMBERING is an explicit [GAP] in doc/modules/mig29/weapons.md §2.1, so 1..6 left-outboard
 * to right-outboard is a FlightBox convention chosen to match the F-16 slot's left-to-right ordering,
 * and it is not presented as a MiG-29 fact. The centreline (station 7 here) carries the PTB-1500 ferry
 * tank only — the 9-12's inboard pylons are not wet (§2.2) — so it is declared for completeness and
 * nothing in the tree loads it.
 *
 * WHAT IS NOT MODELLED, named rather than approximated: the pair-release semantics of §2.4 (this jet
 * selects an INNER or OUTER pair and a quantity switch, not a station step). The F-16's one-store-one-
 * unit path is used instead, which is the SPECIAL case of that switch in SINGLE — the general case
 * would be two FBStoreRelease records in one tick, and no measurement in the tree needs it yet. */
#ifndef FBMIG29SMS_H
#define FBMIG29SMS_H

#include "FBStoresSystem.h"

namespace FlightBox::Modules {

class FBMig29Sms : public Weapons::FBStoresSystem {
public:
  FBMig29Sms();
};

} // namespace FlightBox::Modules
#endif
