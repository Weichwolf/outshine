#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "Check.h"
#include "HeapProbe.h"
#include "Script.h"

using namespace outshine::Test;
namespace S = outshine::Script;

namespace {

constexpr double kFrameBudgetMs = 16.67;
constexpr double kActorShare = 0.20;

class Train final : public S::Host {
public:
  double Where = 0, Speed = 12.5, Step = 1.0 / 60.0;
  int Stops = 0;

  [[nodiscard]] S::Value Global(std::string_view name) override {
    if (name == "self") { return S::Value::OfRef(kSelf); }
    if (name == "dt") { return S::Value::OfNumber(Step); }
    return {};
  }
  [[nodiscard]] S::Value Member(const S::Value &object, std::string_view name) override {
    if (object.What != S::Kind::Ref || object.Ref != kSelf) { return {}; }
    if (name == "at") { return S::Value::OfNumber(Where); }
    if (name == "speed") { return S::Value::OfNumber(Speed); }
    return {};
  }
  [[nodiscard]] bool SetMember(const S::Value &object, std::string_view name,
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

}

int main(void) {

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

  constexpr int kWarmup = 64;
  constexpr int kTicks = 20000;
  for (int at = 0; at < kWarmup; ++at) { (void)tick.Run(train, error); }

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

  CHECK(train.Stops > 0, "the train reached the station and stopped, more than once over the run");
  CHECK(train.Where >= 0 && train.Where <= 100, "and it is somewhere on its own track");

  CHECK(allocated == 0,
        "a tick of a numeric script takes nothing new from the allocator after its first run, so the "
        "frame path holds");
  CHECK(cost.P99Us > 0, "a tick took measurable time, so this is a measurement");
  CHECK(actors >= 200,
        "at least two hundred actors fit in the share of a frame they were given, which is the number "
        "a world of this size needs before the count becomes the reason to change the design");
  return Report();
}
