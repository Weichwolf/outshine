/* FlightBox — FBF16Sms: the F-16's Stores Management Set. Everything the SMS DOES is the generic
 * systems/FBStoresSystem (inventory, the master-arm interlock, the release path, the point-mass/drag
 * carriage effect); what is F-16 about this class is the one thing that genuinely is — WHERE the nine
 * pylons sit on this airframe, and therefore what a store does to its balance.
 *
 * STATION GEOMETRY, and how honest it is. The numbers in the .cpp are in the pinned f16.xml's OWN
 * structural frame, anchored to the references the model itself provides, and nothing else:
 *   - Stations 4 and 6 take the butt line of the model's own external tanks, which its propulsion
 *     section labels "(station 4)" and "(station 6)": BL +-65 in.
 *   - Stations 1/9 sit at the wingtips, i.e. at the model's own half span (<wingspan> 30 ft -> 180 in);
 *     3/7 and 2/8 are spaced between the two anchors.
 *   - Every station shares the model's own CG station (FS -193 in) longitudinally. doc/f16/weapons.md
 *     §4.5 marks the public station data as T4 (community, "not independently confirmed"), so there is
 *     no citable per-station fuselage station — and placing a store at the CG station is the minimal
 *     assumption: it adds mass and drag without inventing a pitching moment nobody can source. The
 *     LATERAL offsets are modelled, because those follow from the wing geometry the model declares.
 *   - z = -30 in hangs the store below the wing: the model's own wing tanks sit at z = -15 and its main
 *     gear at z = -72, so a pylon-carried store between the two is geometry, not a guess.
 * The master arm powers up SAFE (FBStoresSystem's default) and is armed the way a pilot arms it — over
 * the command bus, from the mission's brief. */
#ifndef FBF16SMS_H
#define FBF16SMS_H

#include "FBStoresSystem.h"

namespace FlightBox {

class FBF16Sms : public FBStoresSystem {
public:
  FBF16Sms();
};

} // namespace FlightBox
#endif
