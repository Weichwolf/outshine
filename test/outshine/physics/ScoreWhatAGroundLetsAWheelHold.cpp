#include <cmath>
#include <cstdio>

#include "Check.h"
#include "Rig.h"

namespace {

// A tyre's peak grip is not the tyre's alone. The pair (rubber, surface) has the coefficient,
// and the standard way to carry that is a SURFACE FACTOR multiplying the tyre's own measured
// coefficient:
//
//   mu(pair) = mu(tyre) * g(surface)
//
// with g = 1 on the surface the tyre's mu was measured on -- dry asphalt, by convention, which
// is why the factor is dimensionless and its unit value is a definition rather than a number
// somebody chose. Every published surface table is written this way (dry 1.0, wet ~0.7, gravel
// ~0.6, ice ~0.15), because a tyre measured on one surface has to be usable on the others.
//
// The factor multiplies the PEAK and leaves the load sensitivity alone: mu0*(N/N0)^-k scaled by
// g is (g*mu0)*(N/N0)^-k. So the closed form this case checks is exact and carries no tolerance
// beyond the arithmetic:
//
//   HoldN(g) = g * HoldN(1)                    and therefore
//   Ratio(g) = asked / HoldN(g) = Ratio(1) / g
//
// `Reading::RatioOfHold` is what a contact reports, so the second line is readable from `Bear`
// without opening the tyre.
//
// WHY THIS IS THE FOUNDATION AND NOT A DETAIL. Until this stood, a wheel that left the made
// surface was reported AIRBORNE -- `Footing::Found` was false and the contact was skipped
// entirely. A car that drives onto grass does not take off; it keeps its four contacts and
// loses grip, and it cannot lose grip through a channel that does not exist. `Footing::Friction`
// is that channel.
constexpr double kExactly = 1e-12;

constexpr double kGrassFactor = 0.55;
constexpr double kIceFactor = 0.15;

constexpr double kLoadN = 3900.0;
constexpr double kSlipRad = 0.06;

[[nodiscard]] outshine::Physics::Shearing Tyre() {
  outshine::Physics::Shearing out;
  out.CorneringNPerRad = 55000.0;
  out.Grip = 0.95;
  out.FrictionAtLoadN = kLoadN;
  out.LoadFalloff = 0.15;
  return out;
}

[[nodiscard]] double HeldOn(double factor) {
  using namespace outshine::Physics;
  Shearing on = Tyre();
  on.Grip *= factor;
  return ShedAt(on, kLoadN, kSlipRad, 0.0).HoldN;
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Physics;

  const double onAsphalt = HeldOn(1.0);
  const double onGrass = HeldOn(kGrassFactor);
  const double onIce = HeldOn(kIceFactor);

  std::printf("HOLD  asphalt %9.2f N   grass %9.2f N   ice %9.2f N\n", onAsphalt, onGrass, onIce);
  std::printf("RATIO grass/asphalt %.15f  declared %.15f\n", onGrass / onAsphalt, kGrassFactor);

  CHECK(std::fabs(onGrass / onAsphalt - kGrassFactor) < kExactly,
        "the surface factor multiplies the PEAK exactly: a ground declaring 0.55 lets the same "
        "tyre under the same load hold 0.55 of what it holds on the surface its coefficient was "
        "measured on, and the load sensitivity is untouched by it");
  CHECK(std::fabs(onIce / onAsphalt - kIceFactor) < kExactly,
        "and the same holds at the other end of the table, so the factor is a factor and not a "
        "curve fitted at one point");

  Rig rig;
  rig.Count = 1;
  rig.Mounts[0].AtM[0] = 0.0;
  rig.Mounts[0].AtM[1] = -0.3;
  rig.Mounts[0].AtM[2] = 0.0;
  rig.Mounts[0].Sheds = Tyre();
  rig.Mounts[0].Strut.ReachM = 0.3;
  rig.Mounts[0].Strut.StiffnessNPerM = 90000.0;
  rig.Mounts[0].Strut.DampingNsPerM = 4000.0;
  rig.Mounts[0].Strut.TravelM = 0.15;
  rig.Mounts[0].Steering.Applied.Ratio = 0.0;
  rig.Mounts[0].Spin.Applied.Ratio = 1.0;

  const auto readAt = [&rig](double factor, double askedN) {
    Rigid body;
    body.MassKg = 400.0;
    body.PositionM[1] = 0.28;
    Footing under;
    under.Found = true;
    under.HeightM = 0.0;
    under.Friction = factor;
    Controls with;
    with.AppliedN = askedN;
    Wrench into;
    rig.HeldSlipRad[0] = 0.0;
    return Bear(rig, body, &under, with, into, 0.01);
  };

  const double askedN = 1200.0;
  const Reading onFirm = readAt(1.0, askedN);
  const Reading onLoose = readAt(kGrassFactor, askedN);

  std::printf("CONTACT load %9.2f N   ratio firm %.6f   ratio loose %.6f\n", onFirm.LoadN[0],
              onFirm.RatioOfHold[0], onLoose.RatioOfHold[0]);

  CHECK(onFirm.Touching[0] && onLoose.Touching[0],
        "both arms stand on the ground: the wheel is not airborne on either, which is the whole "
        "point -- a surface change is a change of GRIP and never a change of contact");
  CHECK(onFirm.LoadN[0] == onLoose.LoadN[0],
        "and the vertical load is identical, so the ratio below is the friction moving and "
        "nothing else");
  CHECK(onFirm.RatioOfHold[0] > 0.0 &&
            std::fabs(onLoose.RatioOfHold[0] * kGrassFactor - onFirm.RatioOfHold[0]) < kExactly,
        "**THE GROUND REACHES THE CONTACT**: the same demand on a ground of 0.55 costs exactly "
        "1/0.55 of the hold it costs on 1.0, so a body driven off the made surface loses grip "
        "by what is under the tyre rather than by a boolean about a corridor it is no longer on");

  Covers("physics: the ground under a contact carries a friction and the contact honours it -- "
         "a surface factor multiplying the tyre's own coefficient exactly, at the peak and not "
         "the load curve, so leaving the road is a change of grip and not a loss of contact");
  return Report();
}
