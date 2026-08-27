#include <cmath>
#include <cstdio>
#include <span>
#include <string>

#include "Check.h"
#include "Wayfinding.h"

namespace {

// A ROAD CLASS CARRIES ITS SURFACE, and the surface has to survive the whole way from the way
// that was laid to the wheel that stands on it. The chain is four links and every one of them is
// a place the number can be dropped:
//
//   the way's class -> the network's way -> the node it snaps to -> the leg the route reports
//
// This case walks it end to end, and then puts the one question the chain does not answer by
// itself: WHAT HAPPENS AT A JUNCTION, where one node belongs to two ways of different surfaces?
//
// The oracle is not a convention we chose. A node is a single point at which a wheel will stand,
// and it will stand on ONE surface. Promising the better of the two would let a car brake on a
// gravel side road with asphalt's grip, and a driver that plans on a promise the ground does not
// keep leaves the road. Promising the worse costs a little performance and never lies. In every
// engineering discipline that merges a safety-relevant property across a joint the rule is the
// same and for the same reason: the joint is as strong as its weakest member.
//
// So: MIN over the ways meeting at a node, and the width rule beside it is deliberately the
// opposite -- MAX, because a node has to be wide enough for the widest thing that reaches it.
// The two rules disagree because the two quantities fail in opposite directions, and a case that
// checked only one of them would let somebody "fix" the other into agreement.
constexpr double kSphereM = 6371000.0;
constexpr double kSnapM = 8.0;

constexpr double kAsphalt = 1.0;
constexpr double kGravel = 0.6111;

[[nodiscard]] outshine::Path::WayClass Made(double halfWidthM, double friction) {
  outshine::Path::WayClass out;
  out.HalfWidthM = halfWidthM;
  out.MaxGradient = 0.06;
  out.Friction = friction;
  out.Lanes = 2;
  return out;
}

[[nodiscard]] double LeastFriction(const outshine::Path::Route &over) {
  double least = 1.0e9;
  for (const outshine::Path::Leg &one : over.Legs) {
    least = one.Friction < least ? one.Friction : least;
  }
  return over.Legs.empty() ? -1.0 : least;
}

[[nodiscard]] double WidestHere(const outshine::Path::Route &over) {
  double most = 0.0;
  for (const outshine::Path::Leg &one : over.Legs) {
    most = one.HalfWidthM > most ? one.HalfWidthM : most;
  }
  return most;
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Path;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  {
    Network alone(kSnapM, kSphereM);
    const double run[4] = {48.0, 11.000, 48.0, 11.010};
    alone.Lay(std::span<const double>(run, 4), Made(4.0, kGravel));
    std::string why;
    if (!alone.Weave(why)) {
      Unprepared(("a single way did not weave: " + why).c_str());
      return Report();
    }
    const Route over = alone.Plan(Waypoint{48.0, 11.000}, Waypoint{48.0, 11.010}, 0.0);
    std::printf("ONE WAY, gravel   legs %zu   least friction %.4f   declared %.4f\n",
                over.Legs.size(), LeastFriction(over), kGravel);
    CHECK(!over.Legs.empty() && std::fabs(LeastFriction(over) - kGravel) < 1e-9,
          "**THE SURFACE REACHES THE LEG**: what a way's class declared arrives unchanged at the "
          "route the pilot reads, through the way, the node and the leg, so a wheel can be told "
          "what it is standing on without asking the road what kind it is");
  }

  Network meeting(kSnapM, kSphereM);
  const double asphalt[4] = {48.0, 11.000, 48.0, 11.010};
  const double gravel[4] = {47.998, 11.005, 48.002, 11.005};
  meeting.Lay(std::span<const double>(asphalt, 4), Made(4.0, kAsphalt));
  meeting.Lay(std::span<const double>(gravel, 4), Made(2.5, kGravel));
  const size_t joined = meeting.Cross();
  std::string why;
  if (!meeting.Weave(why)) {
    Unprepared(("the crossing did not weave: " + why).c_str());
    return Report();
  }
  std::printf("TWO WAYS MEET     joined %zu junction(s)\n", joined);

  const Route alongAsphalt =
      meeting.Plan(Waypoint{48.0, 11.000}, Waypoint{48.0, 11.010}, 0.0);
  const Route acrossGravel =
      meeting.Plan(Waypoint{47.998, 11.005}, Waypoint{48.002, 11.005}, 0.0);

  std::printf("  along the asphalt  least friction %.4f   widest half %.2f m\n",
              LeastFriction(alongAsphalt), WidestHere(alongAsphalt));
  std::printf("  across the gravel  least friction %.4f   widest half %.2f m\n",
              LeastFriction(acrossGravel), WidestHere(acrossGravel));

  CHECK(joined > 0,
        "the two ways meet at a junction, so there is a shared node for the rules below to "
        "disagree about");
  CHECK(LeastFriction(alongAsphalt) > 0.0 &&
            LeastFriction(alongAsphalt) <= kGravel + 1e-9,
        "**THE JOINT TAKES THE WORSE SURFACE**: a route down the asphalt reports the GRAVEL's "
        "friction where the two meet, because a node is one point a wheel stands on and it must "
        "not promise grip the ground does not have -- a driver planning on a promise the surface "
        "will not keep leaves the road at exactly the place the promise was made");
  CHECK(WidestHere(alongAsphalt) >= 4.0 - 1e-9,
        "and the WIDTH rule at the same node is the opposite -- the widest way wins -- because "
        "width and grip fail in opposite directions: a node too narrow refuses a vehicle that "
        "fits, a node too grippy sends one off the road. The two rules must disagree, and a case "
        "that checked only one of them would let the other be 'fixed' into agreement");

  Covers("wayfinding: a road class carries its surface friction, that number survives the way, "
         "the node and the leg unchanged, and where two classes share a junction the node takes "
         "the LOWER friction while it takes the GREATER width");
  return Report();
}
