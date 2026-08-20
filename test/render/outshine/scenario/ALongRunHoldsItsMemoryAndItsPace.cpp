#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"
#include "HeapProbe.h"
#include "Live.h"
#include "PreparedRoot.h"
#include "Renderer.h"

namespace {

using outshine::Test::Checked;
using outshine::Test::Report;

constexpr int kSurfaceW = 1280, kSurfaceH = 720;
constexpr double kFrameBudgetMs = 16.67;

constexpr int kRunFrames = 600;

constexpr int kSettleFrames = 60;

constexpr size_t kCeilingBytes = 8u * 1024u * 1024u;

constexpr double kFill = 0.9;
constexpr double kOrbitDegPerFrame = 1.0;
constexpr double kKeyLux = 3.0, kKeyElevationDeg = 35.0, kKeyBearingDeg = -35.0;
constexpr double kAmbient = 0.35;

constexpr const char *kSubject = "BoxAnimated";

[[nodiscard]] std::string EntryPath(const std::string &prepared) {
  std::string text;
  if (std::FILE *file = std::fopen((prepared + "/manifest.json").c_str(), "rb"); file != nullptr) {
    char buffer[4096];
    size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, file)) > 0) { text.append(buffer, got); }
    std::fclose(file);
  }
  const size_t at = text.find("\"entry\"");
  if (at == std::string::npos) { return {}; }
  const size_t open = text.find('"', text.find(':', at));
  const size_t close = open == std::string::npos ? std::string::npos : text.find('"', open + 1);
  if (open == std::string::npos || close == std::string::npos) { return {}; }
  return prepared + "/" + text.substr(open + 1, close - open - 1);
}

[[nodiscard]] outshine::Clients::Declaration Declared(void) {
  outshine::Clients::Declaration out;
  out.SurfaceWidthPx = kSurfaceW;
  out.SurfaceHeightPx = kSurfaceH;
  const char *named = std::getenv("SCENARIO_SUBJECT");
  out.Stands = EntryPath(outshine::Test::PreparedRoot() + "/" +
                         outshine::Test::kPreparedKhronosPrefix +
                         (named != nullptr ? named : kSubject));
  out.Fill = kFill;
  out.OrbitDegPerFrame = kOrbitDegPerFrame;
  out.KeyLux = kKeyLux;
  out.KeyElevationDeg = kKeyElevationDeg;
  out.KeyBearingDeg = kKeyBearingDeg;
  out.Environment[0] = out.Environment[1] = out.Environment[2] = kAmbient;
  return out;
}

struct Distribution {
  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0, MaxMs = 0.0;
};

[[nodiscard]] Distribution Over(std::vector<double> &samples) {
  Distribution out;
  if (samples.empty()) { return out; }
  std::sort(samples.begin(), samples.end());
  const auto at = [&samples](double fraction) {
    return samples[(size_t)(fraction * (double)(samples.size() - 1) + 0.5)];
  };
  out.P50Ms = at(0.50);
  out.P95Ms = at(0.95);
  out.P99Ms = at(0.99);
  out.MaxMs = samples.back();
  return out;
}

}

int main(void) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::string error;
  outshine::Render::Renderer renderer;
  std::unique_ptr<outshine::Clients::Live> live;

  const bool stood = outshine::Clients::Live::Open(renderer, Declared(), nullptr, live, error);
  if (!stood) { std::printf("       %s\n", error.c_str()); }
  CHECK(stood, "the long run's declaration stands a scenario up");
  if (!stood) { return Report(); }
  std::printf("NOTE run = %d frames over %s, %d of them the declared settling, orbiting %.1f deg a "
              "frame\n",
              kRunFrames, kSubject, kSettleFrames, kOrbitDegPerFrame);
  std::printf("NOTE the subject reports %d frames on its own grid\n", live->Frames());

  std::vector<double> paceMs;
  std::vector<size_t> live_;
  paceMs.reserve((size_t)kRunFrames);
  live_.reserve((size_t)kRunFrames);
  bool advanced = true;
  for (int frame = 0; frame < kRunFrames && advanced; ++frame) {
    const auto began = std::chrono::steady_clock::now();
    advanced = live->Advance(error);
    paceMs.push_back(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
    live_.push_back(outshine::HeapProbe::Sample());
  }
  if (!advanced) { std::printf("       %s\n", error.c_str()); }
  CHECK(advanced, "every frame of the long run advances");
  CHECK(live_.size() == (size_t)kRunFrames, "and every one of them was sampled");
  if (live_.size() != (size_t)kRunFrames) { return Report(); }

  constexpr int kWindows = 4;
  const size_t settled = live_[(size_t)kSettleFrames];
  const size_t span = (live_.size() - (size_t)kSettleFrames) / (size_t)kWindows;
  size_t floors[kWindows] = {};
  for (int window = 0; window < kWindows; ++window) {
    const size_t from = (size_t)kSettleFrames + (size_t)window * span;
    size_t least = live_[from];
    for (size_t at = from; at < from + span && at < live_.size(); ++at) {
      least = live_[at] < least ? live_[at] : least;
    }
    floors[window] = least;
  }
  size_t highest = live_[(size_t)kSettleFrames];
  for (size_t at = (size_t)kSettleFrames; at < live_.size(); ++at) {
    highest = live_[at] > highest ? live_[at] : highest;
  }
  const size_t over = highest > floors[0] ? highest - floors[0] : 0;
  const size_t grew = floors[kWindows - 1] > floors[0] ? floors[kWindows - 1] - floors[0] : 0;
  std::printf("NOTE live heap bytes at the settling point %zu, at the end %zu\n", settled,
              live_.back());
  if (std::getenv("SCENARIO_TRACE") != nullptr) {
    for (size_t at = 0; at < live_.size(); at += 25) {
      std::printf("TRACE frame %4zu live %zu\n", at, live_[at]);
    }
  }
  std::printf("NOTE the floor of each quarter of the settled run: %zu %zu %zu %zu\n", floors[0],
              floors[1], floors[2], floors[3]);
  std::printf("NOTE the floor moved %zu bytes over %zu frames = %.4f bytes a frame\n", grew, span * 3,
              (double)grew / (double)(span * 3));
  std::printf("NOTE the highest reading is %zu bytes above the first floor, ceiling %zu\n", over,
              kCeilingBytes);
  std::printf("NOTE the probe's own walk cost %.4f ms on its last sample, over a heap of %zu bytes\n",
              outshine::HeapProbe::SampleCostMs(), live_.back());
  CHECK(over <= kCeilingBytes,
        "a long run holds its memory under a declared ceiling -- unbounded growth is the failure no "
        "distribution over ninety frames can show, and a scenario that leaks is unplayable long "
        "before it is slow");

  std::vector<double> settledPace(paceMs.begin() + kSettleFrames, paceMs.end());
  const Distribution pace = Over(settledPace);
  std::printf("NOTE pace over the settled run ms  p50 %.4f  p95 %.4f  p99 %.4f  max %.4f\n",
              pace.P50Ms, pace.P95Ms, pace.P99Ms, pace.MaxMs);
  std::printf("NOTE budget %.2f ms -- p99 uses %.1f%% of it\n", kFrameBudgetMs,
              100.0 * pace.P99Ms / kFrameBudgetMs);
  CHECK(pace.P99Ms < kFrameBudgetMs,
        "and it holds the frame budget at p99 over its whole length, so nothing drifts into the frame "
        "as the run goes on");

  std::vector<double> early(settledPace.begin(), settledPace.begin() + settledPace.size() / 2);
  std::vector<double> late(settledPace.begin() + settledPace.size() / 2, settledPace.end());
  const Distribution first = Over(early), second = Over(late);
  std::printf("NOTE first half p50 %.4f, second half p50 %.4f, drift %.4f ms\n", first.P50Ms,
              second.P50Ms, second.P50Ms - first.P50Ms);

  return Report();
}
