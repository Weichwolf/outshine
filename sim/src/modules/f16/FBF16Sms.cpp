#include "FBF16Sms.h"

namespace FlightBox::Modules {

namespace {
/* Left wingtip (1) to right wingtip (9); station 5 is the centreline. */
constexpr double kStationXIn = -193.0;   /* the model's own CG station */
constexpr double kStationZIn = -30.0;    /* under the wing, between the model's tank and gear z */
struct Pylon { int Number; double YIn; };
constexpr Pylon kPylons[] = {
    {1, -180.0}, {2, -140.0}, {3, -105.0}, {4, -65.0}, {5, 0.0},
    {6, 65.0}, {7, 105.0}, {8, 140.0}, {9, 180.0},
};
/* WELCHE PYLONE VERROHRT SIND, und das sagt das MODELL selbst: f16.xml deklariert vier Tanks und nennt
 * die beiden letzten "External Tank number 0 (station 4)" und "1 (station 6)". Hier steht nichts
 * Erfundenes, nur diese Zuordnung. doc/modules/stores.md §Spec. */
constexpr int kTankStation[][2] = {{4, 2}, {6, 3}};
} // namespace

FBF16Sms::FBF16Sms() {
  for (const Pylon &p : kPylons) DeclareStation(p.Number, kStationXIn, p.YIn, kStationZIn);
  for (const auto &t : kTankStation) DeclareFuelPlumbing(t[0], t[1]);
}

} // namespace FlightBox::Modules
