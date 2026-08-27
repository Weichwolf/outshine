#include <cmath>
#include <cstdio>

#include "Check.h"
#include "Shear.h"

namespace {

// Two closed forms, and the derivation is the part a reader can check.
//
// ONE -- THE BRUSH. A tyre's contact patch is a row of bristles. Near zero slip every bristle
// grips and the force is linear in slip: F = C*alpha, where C is the cornering stiffness. As
// slip grows the rear of the patch breaks away first, and the sliding fraction grows until the
// whole patch slides at the friction limit mu*N. Integrating a linear pressure distribution over
// the patch gives the Fiala form:
//
//   x = C*alpha / (3*mu*N)                 the fraction of the patch that has broken away
//   F = mu*N * (1 - (1 - x)^3)             for |x| < 1, and mu*N beyond it
//
// Two properties fall straight out of that and are what this case checks:
//
//   as x -> 0,  (1 - (1-x)^3) -> 3x,  so F -> mu*N * 3x = C*alpha    the slope is preserved
//   at x  = 1,  F = mu*N exactly                                     the peak is the limit
//
// A hard clip -- min(C*alpha, mu*N) -- has the same two endpoints and a CORNER between them. The
// corner is the defect: a tyre that is either fully gripping or fully sliding has no progressive
// breakaway, so a car built on it snaps rather than slides.
//
// TWO -- THE LOAD. A tyre's peak friction FALLS as vertical load rises: the contact patch grows
// less than proportionally and the rubber shears at lower specific stress. The standard
// empirical form is a power law,
//
//   mu(N) = mu0 * (N / N0)^(-k)
//
// with mu0 the coefficient measured at the reference load N0 and k the load sensitivity. Its
// consequence is what makes weight transfer matter at all: an axle that gains load gains grip
// LESS than proportionally, so a pair of tyres carrying 60/40 holds less than the same pair
// carrying 50/50. Without it, weight transfer is a picture and not a physics.
constexpr double kAgreesWithin = 1e-12;
constexpr double kSlopeWithin = 1e-3;

constexpr double kFriction = 0.95;
constexpr double kReferenceN = 3900.0;
constexpr double kFalloff = 0.15;
constexpr double kCorneringNPerRad = 55000.0;

[[nodiscard]] outshine::Physics::Slip Declared(double falloff) {
  outshine::Physics::Slip out;
  out.StiffnessNPerRad = kCorneringNPerRad;
  out.Friction = kFriction;
  out.FrictionAtLoadN = kReferenceN;
  out.LoadFalloff = falloff;
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Physics;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Slip tyre = Declared(kFalloff);
  const double holdN = kFriction * kReferenceN;

  // ONE, the slope. At a slip angle small enough that the broken-away fraction is a thousandth,
  // the brush force must be the linear force to within that thousandth.
  {
    const double smallRad = 0.001 * 3.0 * holdN / kCorneringNPerRad;
    const double linearN = kCorneringNPerRad * smallRad;
    const double heldN = Brushed(linearN, holdN);
    const double off = std::fabs(heldN - linearN) / linearN;
    std::printf("AT A THOUSANDTH OF THE PATCH: linear %.6f N, brush %.6f N, off by %.3e\n",
                linearN, heldN, off);
    CHECK(off < kSlopeWithin,
          "**THE BRUSH KEEPS THE CORNERING STIFFNESS**: near zero slip every bristle grips and "
          "the force IS C*alpha -- a model that lost the slope would be a different tyre at the "
          "only place a car spends most of its life");
  }

  // ONE, the peak. At exactly the slip that breaks the whole patch away, the force is exactly
  // the friction limit -- not a hair under it and not a hair over.
  {
    const double peakRad = 3.0 * holdN / kCorneringNPerRad;
    const double heldN = Brushed(kCorneringNPerRad * peakRad, holdN);
    std::printf("AT THE FULL PATCH, alpha = %.6f rad: %.15f N, the limit is %.15f N\n", peakRad,
                heldN, holdN);
    CHECK(std::fabs(heldN - holdN) < kAgreesWithin,
          "and it peaks AT the friction limit, exactly, where the whole patch has broken away");
    CHECK(std::fabs(Brushed(kCorneringNPerRad * peakRad * 4.0, holdN) - holdN) < kAgreesWithin,
          "and holds there beyond it -- a sliding patch cannot shed more than it holds");
  }

  // ONE, the corner. This is what separates the brush from a clip, and it is the reason to have
  // it: halfway to the peak the two disagree by a quarter of the whole force.
  {
    const double halfRad = 1.5 * holdN / kCorneringNPerRad;
    const double linearN = kCorneringNPerRad * halfRad;
    const double brushN = Brushed(linearN, holdN);
    const double clippedN = linearN < holdN ? linearN : holdN;
    const double apart = std::fabs(brushN - clippedN) / holdN;
    std::printf("HALFWAY TO THE PEAK: brush %.4f N, a hard clip %.4f N, apart by %.4f of the "
                "limit\n", brushN, clippedN, apart);
    CHECK(apart > 0.1,
          "and the control is a control: a hard clip and the brush disagree by more than a tenth "
          "of the limit in the middle of the range, so this case can tell them apart");
  }

  // TWO, the load. mu at twice the reference load must be exactly 2^-k of mu at the reference.
  {
    const double atReference = FrictionAt(tyre, kReferenceN);
    const double atTwice = FrictionAt(tyre, 2.0 * kReferenceN);
    const double wanted = kFriction * std::pow(2.0, -kFalloff);
    std::printf("mu AT THE REFERENCE %.1f N: %.15f, DECLARED %.15f\n", kReferenceN, atReference,
                kFriction);
    std::printf("mu AT TWICE IT:            %.15f, the power law says %.15f\n", atTwice, wanted);
    CHECK(std::fabs(atReference - kFriction) < kAgreesWithin,
          "at the reference load the declared coefficient holds exactly -- the reference is what "
          "the number was measured at, so anything else would make the declaration a lie");
    CHECK(std::fabs(atTwice - wanted) < kAgreesWithin,
          "**PEAK FRICTION FALLS AS LOAD RISES**: mu(N) = mu0 * (N/N0)^-k, and this is the whole "
          "reason weight transfer changes what a car can do -- without it an axle that gains "
          "load gains grip in exact proportion and the transfer cancels out of every corner");
    CHECK(atTwice < atReference,
          "and it FALLS rather than rises, which is the direction the sign of the exponent "
          "decides and the one a typo would reverse in silence");
  }

  // TWO, the pair. Two tyres at 50/50 hold more than the same two at 60/40 -- the consequence,
  // stated as the thing a driver feels.
  {
    const double load = 2.0 * kReferenceN;
    const double even = FrictionAt(tyre, 0.5 * load) * 0.5 * load * 2.0;
    const double leaning = FrictionAt(tyre, 0.6 * load) * 0.6 * load +
                           FrictionAt(tyre, 0.4 * load) * 0.4 * load;
    std::printf("TWO TYRES AT 50/50 HOLD %.4f N, THE SAME TWO AT 60/40 HOLD %.4f N\n", even,
                leaning);
    Note("what the pair loses to a 60/40 lean", (even - leaning) / even, "of the whole");
    CHECK(leaning < even,
          "so a pair that leans holds LESS than a pair that does not, which is why weight "
          "transfer is a physics and not a picture");
  }

  // And the control on the whole second half: with the falloff undeclared, mu is flat and the
  // pair loses nothing at all.
  {
    const Slip flat = Declared(0.0);
    const double load = 2.0 * kReferenceN;
    const double even = FrictionAt(flat, 0.5 * load) * 0.5 * load * 2.0;
    const double leaning =
        FrictionAt(flat, 0.6 * load) * 0.6 * load + FrictionAt(flat, 0.4 * load) * 0.4 * load;
    std::printf("WITH NO FALLOFF DECLARED: 50/50 holds %.4f N and 60/40 holds %.4f N\n", even,
                leaning);
    CHECK(std::fabs(even - leaning) < kAgreesWithin,
          "and a tyre that declares no load sensitivity is EXACTLY indifferent to the lean -- "
          "the default changes nothing, so a scenario that declares none gets the model it had");
  }

  Covers("physics: a tyre's lateral force follows the brush characteristic -- the cornering "
         "stiffness at small slip, the friction limit at full breakaway, and a progressive "
         "curve between -- and its peak friction falls with vertical load by a declared power");
  return Report();
}
