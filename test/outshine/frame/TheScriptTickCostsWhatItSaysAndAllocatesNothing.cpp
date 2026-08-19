/* WHAT A SCRIPT COSTS ON THE FRAME PATH, AND WHETHER IT ALLOCATES THERE (board:1448).
 *
 * **A SCRIPT THAT MOVES SOMETHING RUNS EVERY TICK**, which puts it beside every other term the frame
 * budget has to hold. So two questions have numbers here and not opinions: what one tick costs, and
 * how many actors fit in the share of a frame the engine is willing to give them.
 *
 * **THE ALLOCATION CLAIM IS MEASURED AND NOT ASSERTED.** `Names_` and `Held_` keep their capacity
 * across runs, so a numeric script should allocate on its first tick and on none after it -- and
 * *should* is exactly the word an instrument exists to remove. The instrument is the tree's own
 * `HeapProbe`, which reads what the allocator has TAKEN: a number that never falls, so a tick that
 * allocated and freed still shows -- which is the question here, because a free costs a frame just as
 * an allocation does.
 *
 * **THE STEP COMES FROM THE DECLARATION.** The host hands `dt` over; nothing here reads a clock, which
 * is `CLAUDE.md`'s own rule that a result decided by pace is a coupling and not a result. */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Check.h"
#include "HeapProbe.h"
#include "Script.h"

using namespace outshine::Test;
namespace S = outshine::Script;

namespace {

/* [SET] THE FRAME, AND THE SHARE ACTORS MAY TAKE OF IT. A world's actors are not what a frame is FOR
 * either -- the picture is -- and a fifth is the largest slice that leaves the rest recognisable. It
 * is a number somebody chose and it is checked rather than quoted. */
constexpr double kFrameBudgetMs = 16.67;
constexpr double kActorShare = 0.20;

/* A HOST THAT IS A TRAIN. Three words: where it is, how fast it goes, and how long this step is. Every
 * one of them is a value the consumer owns, and the script can reach nothing else -- which is the
 * whole of what *bound to a host* means. */
class Train final : public S::Host {
public:
  double Where = 0, Speed = 12.5, Step = 1.0 / 60.0;
  int Stops = 0;

  [[nodiscard]] S::Value Global(const std::string &name) override {
    if (name == "self") { return S::Value::OfRef(kSelf); }
    if (name == "dt") { return S::Value::OfNumber(Step); }
    return {};
  }
  [[nodiscard]] S::Value Member(const S::Value &object, const std::string &name) override {
    if (object.What != S::Kind::Ref || object.Ref != kSelf) { return {}; }
    if (name == "at") { return S::Value::OfNumber(Where); }
    if (name == "speed") { return S::Value::OfNumber(Speed); }
    return {};
  }
  [[nodiscard]] bool SetMember(const S::Value &object, const std::string &name,
                               const S::Value &to) override {
    if (object.What != S::Kind::Ref || object.Ref != kSelf) { return false; }
    if (name == "at") {
      Where = to.Number;
      return true;
    }
    if (name == "stopped") {
      Stops += to.Truth() ? 1 : 0;
      return true;
    }
    return false;
  }

private:
  static constexpr int kSelf = 1;
};

struct Distribution {
  double P50Us = 0, P95Us = 0, P99Us = 0, MaxUs = 0;
};

[[nodiscard]] Distribution Over(std::vector<double> &samples) {
  Distribution out;
  if (samples.empty()) { return out; }
  std::sort(samples.begin(), samples.end());
  const auto at = [&samples](double fraction) {
    return samples[(size_t)(fraction * (double)(samples.size() - 1) + 0.5)];
  };
  out.P50Us = at(0.50);
  out.P95Us = at(0.95);
  out.P99Us = at(0.99);
  out.MaxUs = samples.back();
  return out;
}

}  // namespace

int main(void) {
  /* A TRAIN THAT DRIVES BY ITSELF AND STOPS AT THE STATION NOW AND THEN. The phase lives in the
   * program, because an actor's program IS its memory. */
  S::Program tick;
  std::string error;
  CHECK(tick.Read("self.at = self.at + self.speed * dt;\n"
                  "if (self.at > 100) { self.at = 0; self.stopped = 1; }\n",
                  error),
        "the train's script reads");
  std::printf("NOTE the script is %zu nodes of the %zu allowed\n", tick.NodeCount(), S::kMaxNodes);

  Train train;
  CHECK(tick.Run(train, error), "and runs");
  std::printf("NOTE one tick takes %zu steps of the %zu allowed\n", tick.Steps(), S::kMaxSteps);

  /* THE WARM-UP IS WHERE THE ALLOCATION IS ALLOWED TO HAPPEN, and the measured phase is where it may
   * not. Splitting them is what turns *should not allocate* into a number. */
  constexpr int kWarmup = 64;
  constexpr int kTicks = 20000;
  for (int at = 0; at < kWarmup; ++at) { (void)tick.Run(train, error); }

  /* THE INSTRUMENT'S OWN MEMORY IS TAKEN BEFORE THE ZERO POINT IS READ. [MEASURED] reserving the
   * sample vector afterwards made the allocator take a 4 MiB chunk to serve it, and the reading blamed
   * the script for 4 MiB it never asked for -- `CLAUDE.md`'s own rule that an instrument in the path is
   * its own field and is never folded into the number. */
  std::vector<double> samples;
  samples.reserve(kTicks);
  const size_t before = outshine::HeapProbe::BreakBytes();
  for (int at = 0; at < kTicks; ++at) {
    const auto began = std::chrono::steady_clock::now();
    const bool ran = tick.Run(train, error);
    const double us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began).count();
    if (!ran) { break; }
    samples.push_back(us);
  }
  const size_t allocated = outshine::HeapProbe::BreakBytes() - before;

  const Distribution cost = Over(samples);
  const double allowed = kFrameBudgetMs * kActorShare;
  const double actors = cost.P99Us > 0 ? allowed * 1000.0 / cost.P99Us : 0;

  std::printf("NOTE population = %zu ticks of one actor, the step declared and no clock read\n",
              samples.size());
  std::printf("NOTE script tick us   p50 %.3f  p95 %.3f  p99 %.3f  max %.3f\n", cost.P50Us,
              cost.P95Us, cost.P99Us, cost.MaxUs);
  std::printf("NOTE budget %.2f ms, share [SET] %.0f%%, allowed %.3f ms -> %.0f actors at p99\n",
              kFrameBudgetMs, kActorShare * 100.0, allowed, actors);
  std::printf("NOTE bytes the allocator took during the measured ticks = %zu\n", allocated);

  /* THE TRAIN MOVED AND STOPPED, which is what says the numbers above are about work and not about an
   * interpreter returning early. */
  CHECK(train.Stops > 0, "the train reached the station and stopped, more than once over the run");
  CHECK(train.Where >= 0 && train.Where <= 100, "and it is somewhere on its own track");

  /* **NOTHING IS ALLOCATED ON THE FRAME PATH.** A single allocation here is a claim in `CLAUDE.md`
   * turning false, and it would turn false silently -- a tick that allocates costs a mean nobody
   * notices and a p99 somebody does. */
  CHECK(allocated == 0,
        "a tick of a numeric script takes nothing new from the allocator after its first run, so the "
        "frame path holds");
  CHECK(cost.P99Us > 0, "a tick took measurable time, so this is a measurement");
  CHECK(actors >= 200,
        "at least two hundred actors fit in the share of a frame they were given, which is the number "
        "a world of this size needs before the count becomes the reason to change the design");
  return Report();
}
