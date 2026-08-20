#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"
#include "Live.h"
#include "PreparedRoot.h"
#include "Renderer.h"

namespace {

using outshine::Test::Checked;
using outshine::Test::Report;

constexpr int kSurfaceW = 1280, kSurfaceH = 720;

constexpr double kFill = 0.9;

constexpr double kKeyLux = 3.0, kKeyElevationDeg = 35.0, kKeyBearingDeg = -35.0;
constexpr double kAmbient = 0.35;

constexpr const char *kMoving = "BoxAnimated";
constexpr const char *kStill = "Box";

[[nodiscard]] std::string Prepared(const char *name) {
  return outshine::Test::PreparedRoot() + "/" + outshine::Test::kPreparedKhronosPrefix + name;
}

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

[[nodiscard]] outshine::Clients::Declaration Declared(const char *name) {
  outshine::Clients::Declaration out;
  out.SurfaceWidthPx = kSurfaceW;
  out.SurfaceHeightPx = kSurfaceH;
  out.Stands = EntryPath(Prepared(name));
  out.Fill = kFill;
  out.KeyLux = kKeyLux;
  out.KeyElevationDeg = kKeyElevationDeg;
  out.KeyBearingDeg = kKeyBearingDeg;
  out.Environment[0] = out.Environment[1] = out.Environment[2] = kAmbient;
  return out;
}

constexpr int kFromX = 0, kToX = kSurfaceW, kStepX = 3;
constexpr int kFromY = 0, kToY = kSurfaceH, kStepY = 3;

[[nodiscard]] size_t Population(void) {
  size_t count = 0;
  for (int y = kFromY; y < kToY; y += kStepY) {
    for (int x = kFromX; x < kToX; x += kStepX) { ++count; }
  }
  return count;
}

[[nodiscard]] size_t Differing(const std::vector<uint8_t> &left, const std::vector<uint8_t> &right) {
  if (left.size() != right.size() || left.empty()) { return 0; }
  size_t differ = 0;
  for (int y = kFromY; y < kToY; y += kStepY) {
    for (int x = kFromX; x < kToX; x += kStepX) {
      const size_t at = (((size_t)y * (size_t)kSurfaceW) + (size_t)x) * 4u;
      differ += left[at] != right[at] || left[at + 1] != right[at + 1] ||
                        left[at + 2] != right[at + 2]
                    ? 1u
                    : 0u;
    }
  }
  return differ;
}

[[nodiscard]] bool Frame(outshine::Clients::Live &live, outshine::Render::Renderer &renderer,
                         std::vector<uint8_t> &into, std::string &error) {
  return live.Advance(error) &&
         renderer.ReadPixels(into) == outshine::Render::ReadState::Ready;
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

[[nodiscard]] Distribution CostOf(outshine::Clients::Live &live, int frames, std::string &error) {
  std::vector<double> samples;
  samples.reserve((size_t)frames);
  for (int at = 0; at < frames; ++at) {
    const auto began = std::chrono::steady_clock::now();
    const bool advanced = live.Advance(error);
    samples.push_back(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
    if (!advanced) { break; }
  }
  return Over(samples);
}

}

int main(void) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::string error;
  outshine::Render::Renderer renderer;
  std::unique_ptr<outshine::Clients::Live> live;

  std::printf("NOTE population = %zu samples over %dx%d, stride %dx%d\n", Population(), kSurfaceW,
              kSurfaceH, kStepX, kStepY);

  std::printf("NOTE %s stands from %s\n", kMoving, Declared(kMoving).Stands.c_str());
  const bool moving =
      outshine::Clients::Live::Open(renderer, Declared(kMoving), nullptr, live, error);
  if (!moving) { std::printf("       %s\n", error.c_str()); }
  CHECK(moving, "an animated case stands a scenario up");
  if (!moving) { return Report(); }
  std::printf("NOTE %s reports %d frames on its own grid\n", kMoving, live->Frames());
  CHECK(live->Frames() > 1,
        "and it reports more than one frame, derived from the document's own animation");

  std::vector<uint8_t> first, second, wrapped;
  CHECK(Frame(*live, renderer, first, error), "the first frame of it comes off the device");
  const int firstAt = live->At();
  CHECK(Frame(*live, renderer, second, error), "and so does the next");
  const size_t moved = Differing(first, second);
  std::printf("NOTE frame %d against frame %d: %zu of %zu samples differ\n", firstAt, live->At(),
              moved, Population());
  CHECK(moved > 0,
        "two advances of an animated scenario produce two different pictures -- a body that stopped "
        "moving draws the same frame forever, which no still image can show");

  bool lapped = false;
  for (int left = live->Frames(); left > 0 && !lapped; --left) {
    if (!live->Advance(error)) { break; }
    lapped = live->At() == firstAt;
  }
  CHECK(lapped, "advancing a full lap comes back to the frame the first picture was taken at");
  CHECK(renderer.ReadPixels(wrapped) == outshine::Render::ReadState::Ready,
        "and its picture comes off the device");
  const size_t drifted = Differing(first, wrapped);
  std::printf("NOTE frame %d against frame %d one lap later: %zu of %zu samples differ\n", firstAt,
              live->At(), drifted, Population());
  CHECK(drifted == 0,
        "and it is the same picture -- the frame grid is the document's own length over the declared "
        "rate, so a lap that landed anywhere else would be a grid this runtime invented");

  const Distribution animated = CostOf(*live, 120, error);
  std::printf("NOTE animated advance ms  p50 %.4f  p95 %.4f  p99 %.4f  max %.4f\n", animated.P50Ms,
              animated.P95Ms, animated.P99Ms, animated.MaxMs);

  std::vector<uint8_t> held, again;
  const bool standing =
      outshine::Clients::Live::Open(renderer, Declared(kStill), nullptr, live, error);
  if (!standing) { std::printf("       %s\n", error.c_str()); }
  CHECK(standing, "a still case stands a scenario up");
  if (standing) {
    std::printf("NOTE %s reports %d frame\n", kStill, live->Frames());
    CHECK(live->Frames() == 1, "and reports one frame, because its document declares no animation");
    CHECK(Frame(*live, renderer, held, error), "its first frame comes off the device");
    CHECK(Frame(*live, renderer, again, error), "and so does its second");
    const size_t stillMoved = Differing(held, again);
    std::printf("NOTE still frame against still frame: %zu of %zu samples differ\n", stillMoved,
                Population());
    CHECK(stillMoved == 0,
          "two advances of a still scenario produce the same picture -- which is what says the body "
          "is still being drawn and not merely no longer being uploaded");
    const Distribution kept = CostOf(*live, 120, error);
    std::printf("NOTE still advance ms     p50 %.4f  p95 %.4f  p99 %.4f  max %.4f\n", kept.P50Ms,
                kept.P95Ms, kept.P99Ms, kept.MaxMs);
    std::printf("NOTE a pose costs p50 %.4f ms more than standing still, which is the one legitimate "
                "per-frame upload in this engine\n",
                animated.P50Ms - kept.P50Ms);
  }

  return Report();
}
