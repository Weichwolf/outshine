#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Compiled.h"
#include "RenderCatalogue.h"

// WHAT A PLAN IS COMPILED FROM, AND WHAT ORDER IT COMES OUT IN.
//
// Unreal's RDG compiles a pass graph from declared resource reads and writes and culls a pass
// nothing reads; RAGE runs fixed passes. outshine takes Unreal's answer, and `Compiled::Compile`
// has done so for some time -- but board:1941 recorded the awkward half of that: the code does it
// and NO case asserts it. A behaviour nothing pins is a behaviour the next refactor may drop
// silently, and the plan is where a silent drop turns into a black frame.
//
// Three claims, and none of them is about a number this tree chose:
//
//   PULLED    a spec names the stages it WANTS; the plan holds those plus every stage the read
//             edges require to feed them. If the plan held exactly what was named, it would be a
//             list rather than a compilation.
//   REFUSED   a content stage whose output nothing in the plan reads is refused BY NAME. The
//             alternative is a stage that runs, costs a pass, and contributes to no pixel.
//   ORDERED   `Order()` runs producers before consumers. This is graph-theoretic and not a
//             preference: a consumer that runs first reads whatever the last frame left, which is
//             the class of defect that looks like flicker and reads like a driver bug.
//
// The catalogue already carries `TopologicalOrderHolds()` as a static_assert -- the stage
// ENUMERATION is a linear extension of the edge graph. That is half the sentence. This case
// carries the other half: that the compiled order follows the enumeration, so the two together
// mean a producer cannot run after its consumer.

namespace {

using namespace outshine::Render;

[[nodiscard]] std::string Named(const std::vector<Stage> &order) {
  std::string out;
  for (const Stage one : order) { out += (out.empty() ? "" : " "); out += Row(one).Name; }
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // PULLED. Two content stages are named and four stand: `tonemap` and `present` arrive because
  // `Surface` is requested, `present` reads what `tonemap` writes and `tonemap` reads the scene
  // `subjects` draws into. Nothing named them.
  PlanSpec asked;
  asked.Outputs = {Resource::FrameTex, Resource::Surface};
  asked.Content = {Stage::Subjects, Stage::Overlay};
  const auto plan = Compiled::Compile(asked);
  if (!plan) {
    Unprepared(plan.error().c_str());
    return Report();
  }
  const std::vector<Stage> &order = (*plan)->Order();
  std::printf("  named 2, compiled %zu: %s\n", order.size(), Named(order).c_str());
  CHECK(order.size() > asked.Content.size(),
        "**A PLAN IS PULLED THROUGH THE EDGES, NOT LISTED**: a spec names what it wants drawn and "
        "the compiler adds every stage the read edges require to feed it. A plan holding exactly "
        "what was named would be a list, and a list cannot drop a stage nothing reads");

  // REFUSED, and by name. A plan whose only output is the meter cannot use what `sky` draws, so
  // sky would run, cost a raster pass and contribute to no pixel the plan asks for.
  PlanSpec unread;
  unread.Outputs = {Resource::Meter};
  unread.Content = {Stage::AutoExposure, Stage::Sky};
  const auto refused = Compiled::Compile(unread);
  const bool named = !refused && refused.error().find("sky") != std::string::npos;
  std::printf("  a stage nothing reads:  %s\n",
              refused ? "COMPILED" : refused.error().c_str());
  CHECK(named,
        "**A CONTENT STAGE NOTHING READS IS REFUSED BY NAME**: it would run, cost a pass and reach "
        "no pixel this plan asks for. Naming the row is what makes the refusal actionable -- a "
        "plan that simply dropped it would render a frame nobody could explain");

  // ORDERED. Every read of a resource the plan HOLDS is satisfied by a producer standing EARLIER
  // in the compiled order. Checked against the catalogue rather than against a remembered
  // sequence, so it holds for any plan and not only this one.
  //
  // A read of a resource the plan does NOT hold is a different question and this check must not
  // answer it: nothing produces it, so "the producer ran too late" is not the failure available.
  // The first version of this case did conflate them and went red on four such reads. They are
  // counted and printed instead, because what they measure is board:1941's still-open predicate --
  // a resource a plan stops writing is cleared or declared stale, never left standing. In this
  // minimal plan there are four of them, and `subjects` reading an unheld shadowAtlas is the one
  // that reaches a pixel.
  size_t backwards = 0;
  size_t unheld = 0;
  const char *first = "";
  for (size_t at = 0; at < order.size(); ++at) {
    const StageRow &row = Row(order[at]);
    for (size_t edge = 0; edge < kMaxEdges && row.Reads[edge] != kNoEdge; ++edge) {
      const Resource wanted = row.Reads[edge];
      if (Row(wanted).Kind == ResourceKind::Given) { continue; }
      if (!(*plan)->Holds(wanted)) {
        ++unheld;
        continue;
      }
      bool fed = false;
      for (size_t before = 0; before < at; ++before) {
        if (Produces(order[before], wanted)) { fed = true; break; }
      }
      if (!fed) {
        ++backwards;
        if (*first == '\0') { first = row.Name; }
      }
    }
  }
  std::printf("  held reads fed earlier: %zu unfed%s%s   (and %zu read(s) of a resource this "
              "plan does not hold at all)\n", backwards, backwards > 0 ? ", first at " : "",
              first, unheld);
  CHECK(backwards == 0,
        "**A PRODUCER RUNS BEFORE ITS CONSUMER**: a stage reading a derived resource no earlier "
        "stage wrote reads whatever the last frame left in it. The catalogue's "
        "TopologicalOrderHolds() static_assert says the ENUMERATION is a linear extension of the "
        "edge graph; this says the compiled order follows the enumeration, and only both together "
        "make the guarantee");

  Covers("the render plan: a spec is COMPILED through the catalogue's resource edges rather than "
         "listed, a content stage nothing reads is refused by name, and the compiled order runs "
         "every producer before its consumer");
  return Report();
}
