#include "FBF16Sms.h"

namespace FlightBox {

namespace {
/* The nine pylons, left wingtip (1) to right wingtip (9) — see FBF16Sms.h for where every number comes
 * from. Station 5 is the centreline. */
constexpr double kStationXIn = -193.0;   /* the model's own CG station */
constexpr double kStationZIn = -30.0;    /* under the wing, between the model's tank and gear z */
struct Pylon { int Number; double YIn; };
constexpr Pylon kPylons[] = {
    {1, -180.0}, {2, -140.0}, {3, -105.0}, {4, -65.0}, {5, 0.0},
    {6, 65.0}, {7, 105.0}, {8, 140.0}, {9, 180.0},
};
} // namespace

FBF16Sms::FBF16Sms() {
  for (const Pylon &p : kPylons) DeclareStation(p.Number, kStationXIn, p.YIn, kStationZIn);
}

} // namespace FlightBox
