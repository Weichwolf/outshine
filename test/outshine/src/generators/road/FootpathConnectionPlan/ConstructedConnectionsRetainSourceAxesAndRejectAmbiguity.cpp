#include "FootpathConnectionPlan.h"
#include "Check.h"

#include <array>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

FootpathSource Source(std::span<const EastNorth> points, FootpathKind kind) {
  return {.Axis = points,
          .HalfWidthM = .75,
          .Kind = kind,
          .Layer = 0,
          .Bridge = false,
          .Tunnel = false,
          .FootAccess = true,
          .Ends = {FootpathEndRole::Connectable, FootpathEndRole::Connectable}};
}

void ForwardTConnectionsKeepEveryOriginalPoint() {
  using namespace outshine::Test;
  const std::array steps{EastNorth{0, -4}, EastNorth{0, -.282}};
  const std::array target{EastNorth{-4, 0}, EastNorth{4, 0}};
  const std::array sources{Source(steps, FootpathKind::Steps), Source(target, FootpathKind::Path)};
  const auto plan = FootpathConnectionPlan::Prepare(sources);
  CHECK(plan.has_value(), "a bounded forward T proposal is prepared");
  if (!plan) { return; }
  CHECK(plan->Connections().size() == 1, "only the end facing the target connects");
  if (plan->Connections().empty()) { return; }
  const auto &connection = plan->Connections().front();
  CHECK(connection.Branch == 0 && connection.End == 1 && connection.Target == 1,
        "the proposal retains both input-source indices and the actual endpoint");
  CHECK_NEAR(connection.AdvanceM, .282, 1e-15, "m", "only the required forward extension is added");
  CHECK(!connection.PositionBoundsKnown, "a width/reach recipe does not invent a provider bound");
  const std::vector<EastNorth> branch{steps[0], steps[1], {0, 0}};
  const std::vector<EastNorth> crossing{target[0], {0, 0}, target[1]};
  CHECK(plan->Axis(0, steps) == branch, "the branch keeps its original end before the extension");
  CHECK(plan->Axis(1, target) == crossing,
        "the target is cut without replacing either original vertex");
  CHECK(plan->Work().CandidateTests <= 2, "the spatial query skips the distant branch end");
}

void ExactVerticesWinWithoutFalseOriginalNodeIdentity() {
  using namespace outshine::Test;
  const std::array steps{EastNorth{0, -4}, EastNorth{0, 0}};
  const std::array target{EastNorth{-4, 0}, EastNorth{0, 0}, EastNorth{4, 0}};
  const std::array further{EastNorth{-4, .25}, EastNorth{4, .25}};
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
  CHECK(plan->Axis(1, target) == std::vector<EastNorth>(target.begin(), target.end()),
        "an exact internal target knot is not duplicated");
}

void MultiplePlausibleTargetsStayUnconnected() {
  using namespace outshine::Test;
  const std::array steps{EastNorth{0, -4}, EastNorth{0, -.282}};
  const std::array first{EastNorth{-4, 0}, EastNorth{4, 0}};
  const std::array second{EastNorth{-4, .1}, EastNorth{4, .1}};
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
  return outshine::Test::Report();
}
