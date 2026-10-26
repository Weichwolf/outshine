#include "FootpathConnectionPlan.h"
#include "Check.h"

#include <array>
#include <span>
#include <optional>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

constexpr double kHalfWidthM = .75;
constexpr double kArithmeticErrorM = 1e-15;
constexpr double kGapM = .282;
constexpr double kFurtherM = .25;
constexpr double kFixedEndM = .2;
constexpr double kAmbiguousGapM = .1;

EastNorth At(double east, double north) {
  return {.EastM = east, .NorthM = north};
}

FootpathSource Source(std::span<const EastNorth> points, FootpathKind kind) {
  return {.Axis = points,
          .HalfWidthM = kHalfWidthM,
          .Kind = kind,
          .Layer = 0,
          .Bridge = false,
          .Tunnel = false,
          .FootAccess = true,
          .Ends = {FootpathEndRole::Connectable, FootpathEndRole::Connectable},
          .PositionErrorBoundM = std::nullopt};
}

std::vector<EastNorth> Positions(std::span<const FootpathAxisVertex> vertices) {
  std::vector<EastNorth> points;
  for (const auto &vertex : vertices) { points.push_back(vertex.At); }
  return points;
}

void ForwardTConnectionsKeepEveryOriginalPoint() {
  using namespace outshine::Test;
  const std::array steps{At(0, -4), At(0, -kGapM)};
  const std::array target{At(-4, 0), At(4, 0)};
  const std::array sources{Source(steps, FootpathKind::Steps), Source(target, FootpathKind::Path)};
  const auto plan = FootpathConnectionPlan::Prepare(sources);
  CHECK(plan.has_value(), "a bounded forward T proposal is prepared");
  if (!plan) { return; }
  CHECK(plan->Connections().size() == 1, "only the end facing the target connects");
  if (plan->Connections().empty()) { return; }
  const auto &connection = plan->Connections().front();
  CHECK(connection.Branch == 0 && connection.End == 1 && connection.Target == 1,
        "the proposal retains both input-source indices and the actual endpoint");
  CHECK_NEAR(connection.AdvanceM,
             kGapM,
             kArithmeticErrorM,
             "m",
             "only the required forward extension is added");
  CHECK(!connection.PositionBoundsKnown, "a width/reach recipe does not invent a provider bound");
  const std::vector<EastNorth> branch{steps[0], steps[1], At(0, 0)};
  const std::vector<EastNorth> crossing{target[0], At(0, 0), target[1]};
  const auto branchAxis = plan->Axis(0, steps);
  const auto targetAxis = plan->Axis(1, target);
  CHECK(Positions(branchAxis) == branch, "the branch keeps its original end before the extension");
  CHECK(Positions(targetAxis) == crossing,
        "the target is cut without replacing either original vertex");
  CHECK(branchAxis[1].Original == 1 && branchAxis[1].Connection == FootpathAxisVertex::kNoIndex,
        "the old branch end retains its original attributes independently of the new join");
  CHECK(branchAxis[2].Original == FootpathAxisVertex::kNoIndex && branchAxis[2].Connection == 0 &&
            targetAxis[1].Connection == 0,
        "both new vertices identify the same interpolated target connection");
  CHECK(plan->Work().CandidateTests <= 2, "the spatial query skips the distant branch end");
}

void ExactVerticesWinWithoutFalseOriginalNodeIdentity() {
  using namespace outshine::Test;
  const std::array steps{At(0, -4), At(0, 0)};
  const std::array target{At(-4, 0), At(0, 0), At(4, 0)};
  const std::array further{At(-4, kFurtherM), At(4, kFurtherM)};
  const std::array sources{Source(steps, FootpathKind::Steps),
                           Source(target, FootpathKind::Path),
                           Source(further, FootpathKind::Path)};
  const auto plan = FootpathConnectionPlan::Prepare(sources);
  CHECK(plan && plan->Connections().size() == 1,
        "two adjacent target segments describe one exact contact");
  if (!plan || plan->Connections().empty()) { return; }
  const auto &connection = plan->Connections().front();
  CHECK(connection.Target == 1 && connection.JoinedAt == target[1],
        "the exact target vertex wins over a further extension");
  CHECK(connection.AdvanceM == 0, "an existing coordinate does not extend the source");
  const auto axis = plan->Axis(1, target);
  CHECK(Positions(axis) == std::vector<EastNorth>(target.begin(), target.end()),
        "an exact internal target knot is not duplicated");
  CHECK(axis[1].Original == 1 && axis[1].Connection == 0,
        "a target knot carries both its original identity and the joining connection");
  const auto branch = plan->Axis(0, steps);
  CHECK(branch.back().Original == 1 && branch.back().Connection == 0,
        "a coincident branch end binds height and node identity without an extra vertex");
}

void FixedConnectionsAndFragmentsAreNotExtended() {
  using namespace outshine::Test;
  const std::array steps{At(0, -kFixedEndM), At(0, kFixedEndM)};
  const std::array target{At(-4, 0), At(4, 0)};
  auto branch = Source(steps, FootpathKind::Steps);
  branch.Ends = {FootpathEndRole::Fixed, FootpathEndRole::Fragment};
  const std::array sources{branch, Source(target, FootpathKind::Path)};
  const auto plan = FootpathConnectionPlan::Prepare(sources);
  CHECK(plan && plan->Connections().empty(), "existing bindings and tile fragments stay intact");
  if (!plan) { return; }
  CHECK(plan->Endpoints()[0].Disposition == FootpathEndDisposition::FixedBoundary &&
            plan->Endpoints()[1].Disposition == FootpathEndDisposition::Fragment,
        "the reason an endpoint is protected remains explicit");
  CHECK(plan->Work().Queries == 0 && plan->Work().IndexedSegments == 0,
        "protected endpoints do not trigger spatial-index construction");
}

void MultiplePlausibleTargetsStayUnconnected() {
  using namespace outshine::Test;
  const std::array steps{At(0, -4), At(0, -kGapM)};
  const std::array first{At(-4, 0), At(4, 0)};
  const std::array second{At(-4, kAmbiguousGapM), At(4, kAmbiguousGapM)};
  const std::array sources{Source(steps, FootpathKind::Steps),
                           Source(first, FootpathKind::Path),
                           Source(second, FootpathKind::Path)};
  const auto plan = FootpathConnectionPlan::Prepare(sources);
  CHECK(plan && plan->Connections().empty(),
        "two noncoincident targets are not guessed into one connection");
  if (plan) {
    CHECK(plan->Endpoints()[1].Disposition == FootpathEndDisposition::AmbiguousTargets,
          "the unused endpoint records its actual ambiguity");
  }
}
}

int main() {
  ForwardTConnectionsKeepEveryOriginalPoint();
  ExactVerticesWinWithoutFalseOriginalNodeIdentity();
  MultiplePlausibleTargetsStayUnconnected();
  FixedConnectionsAndFragmentsAreNotExtended();
  return outshine::Test::Report();
}
