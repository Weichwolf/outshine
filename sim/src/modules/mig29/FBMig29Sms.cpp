#include "FBMig29Sms.h"

namespace FlightBox::Modules {

namespace {
/* The deck's own references (<mod>/aircraft/mig29/mig29.xml): design CG at station 366 in, wingtips at
 * y = +-223.6 in (half of the documented 37.27 ft span). Every pylon sits at the CG station
 * longitudinally — the same minimal assumption the F-16 slot makes, and for the same reason: no source
 * gives a per-station fuselage station, so a loadout produces no pitching moment and the LATERAL
 * offsets are the part that is modelled. The three lateral positions are [SET] fractions of the half
 * span (0.76 / 0.56 / 0.36), evenly spread between the tip and the fuselage side; §2.1 marks the whole
 * station map as [GAP] below "six wing pylons plus a centreline". */
constexpr double kStationXIn = 366.0;
constexpr double kStationZIn = -40.0;   /* under the wing, above the gear contact z of -86.6 */
struct Pylon { int Number; double YIn; };
constexpr Pylon kPylons[] = {
    {1, -170.0}, {2, -125.0}, {3, -80.0},
    {4, 80.0}, {5, 125.0}, {6, 170.0},
    {7, 0.0},   /* centreline: PTB-1500 ferry tank only (§2.2), declared but never loaded here */
};
} // namespace

FBMig29Sms::FBMig29Sms() {
  for (const Pylon &p : kPylons) DeclareStation(p.Number, kStationXIn, p.YIn, kStationZIn);
}

} // namespace FlightBox::Modules
