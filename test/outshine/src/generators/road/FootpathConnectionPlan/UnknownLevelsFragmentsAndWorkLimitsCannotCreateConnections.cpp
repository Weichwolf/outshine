#include "FootpathConnectionPlan.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <stop_token>
#include <optional>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double kGapM = .282;
  constexpr double kPositionBoundM = .05;
  constexpr size_t kDenseSources = 101;
  const std::array steps{EastNorth{.EastM = 0, .NorthM = -4},
                         EastNorth{.EastM = 0, .NorthM = -kGapM}};
  const std::array path{EastNorth{.EastM = -4, .NorthM = 0}, EastNorth{.EastM = 4, .NorthM = 0}};
  const FootpathSource branch{.Axis = steps,
                              .HalfWidthM = .75,
                              .Kind = FootpathKind::Steps,
                              .Layer = 0,
                              .Bridge = false,
                              .Tunnel = false,
                              .FootAccess = true,
                              .Ends = {FootpathEndRole::Connectable, FootpathEndRole::Connectable},
                              .PositionErrorBoundM = std::nullopt};
  const FootpathSource target{.Axis = path,
                              .HalfWidthM = .75,
                              .Kind = FootpathKind::Path,
                              .Layer = 0,
                              .Bridge = false,
                              .Tunnel = false,
                              .FootAccess = true,
                              .PositionErrorBoundM = std::nullopt};
  std::array sources{branch, target};
  for (const auto role :
       {FootpathEndRole::Unknown, FootpathEndRole::Terminal, FootpathEndRole::Fragment}) {
    sources[0].Ends[1] = role;
    const auto plan = FootpathConnectionPlan::Prepare(sources);
    CHECK(plan && plan->Connections().empty(), "unqualified ends cannot be extended");
  }
  sources = {branch, target};
  sources[1].Layer = 1;
  auto plan = FootpathConnectionPlan::Prepare(sources);
  CHECK(plan && plan->Connections().empty(),
        "coincident planar footprints do not join different levels");
  sources = {branch, target};
  sources[0].Bridge.reset();
  plan = FootpathConnectionPlan::Prepare(sources);
  CHECK(plan && plan->Connections().empty(), "unknown bridge state is not a known ground path");
  if (plan) { CHECK(plan->Work().IndexedSegments == 0, "no eligible ends build no spatial index"); }
  sources = {branch, target};
  sources[0].PositionErrorBoundM = kPositionBoundM;
  sources[1].PositionErrorBoundM = kPositionBoundM;
  plan = FootpathConnectionPlan::Prepare(sources);
  CHECK(plan && plan->Connections().empty(),
        "known positional bounds further limit the extension recipe");
  sources = {branch, target};
  plan = FootpathConnectionPlan::Prepare(sources, {}, {.Candidates = 0});
  CHECK(plan && plan->Connections().empty(),
        "an exhausted candidate budget does not guess a target");
  if (plan) {
    CHECK(plan->Work().CandidateTests == 0 && plan->Work().BudgetRefusals == 1,
          "the spatial query stops before testing an over-budget candidate list");
  }
  std::vector<FootpathSource> dense(kDenseSources, target);
  dense.front() = branch;
  plan = FootpathConnectionPlan::Prepare(dense, {}, {.Candidates = 5});
  CHECK(plan && plan->Connections().empty(),
        "a dense ambiguous neighbourhood cannot exceed its work budget");
  if (plan) {
    CHECK(plan->Work().MaximumCandidates == 6 && plan->Work().CandidateTests == 0,
          "the native query retains only the budget plus one exhaustion witness, not all hundred "
          "targets");
  }
  std::stop_source stop;
  stop.request_stop();
  plan = FootpathConnectionPlan::Prepare(sources, {}, {}, stop.get_token());
  CHECK(!plan && plan.error() == FootpathConnectionError::Canceled,
        "canceled preparations remain canceled");
  plan = FootpathConnectionPlan::Prepare(sources, {}, {.Segments = 0});
  CHECK(!plan && plan.error() == FootpathConnectionError::CapacityExceeded,
        "the segment cap is enforced before indexing");
  return Report();
}
