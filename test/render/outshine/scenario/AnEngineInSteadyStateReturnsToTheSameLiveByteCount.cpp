#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"
#include "Heap.h"
#include "Live.h"
#include "PreparedRoot.h"
#include "Renderer.h"

namespace {

using outshine::Test::Checked;
using outshine::Test::Report;

constexpr int kSurfaceW = 1280, kSurfaceH = 720;

constexpr int kRunFrames = 500;

constexpr int kSettleFrames = 250;

constexpr double kFill = 0.9;
constexpr double kOrbitDegPerFrame = 1.0;
constexpr double kKeyLux = 3.0, kKeyElevationDeg = 35.0, kKeyBearingDeg = -35.0;
constexpr double kAmbient = 0.35;

constexpr long long kHorizonFrames = 100LL * 3600LL * 60LL;

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
  out.Stands = EntryPath(outshine::Test::PreparedRoot() + "/" +
                         outshine::Test::kPreparedKhronosPrefix + kSubject);
  out.Fill = kFill;
  out.OrbitDegPerFrame = kOrbitDegPerFrame;
  out.KeyLux = kKeyLux;
  out.KeyElevationDeg = kKeyElevationDeg;
  out.KeyBearingDeg = kKeyBearingDeg;
  out.Environment[0] = out.Environment[1] = out.Environment[2] = kAmbient;
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
  CHECK(stood, "the steady-state declaration stands a scenario up");
  if (!stood) { return Report(); }
  std::printf("NOTE run = %d frames over %s, %d of them the declared settling, %d frames on the "
              "subject's own grid\n",
              kRunFrames, kSubject, kSettleFrames, live->Frames());

  std::vector<size_t> held;
  held.reserve((size_t)kRunFrames);

  size_t tookOn[4] = {}, worstOf[4] = {};
  bool advanced = true;
  for (int frame = 0; frame < kRunFrames && advanced; ++frame) {
    advanced = live->Advance(error);
    held.push_back(outshine::Heap::LiveBytes());
    if (frame < kSettleFrames) { continue; }
    const ptrdiff_t phase[4] = {(ptrdiff_t)outshine::Clients::Live::TookPosing(),
                                (ptrdiff_t)outshine::Clients::Live::TookSubmitting(),
                                (ptrdiff_t)outshine::Clients::Live::TookAiming(),
                                (ptrdiff_t)outshine::Clients::Live::TookDrawing()};
    for (int at = 0; at < 4; ++at) {
      if (phase[at] == 0) { continue; }
      ++tookOn[at];
      const size_t size = (size_t)(phase[at] < 0 ? -phase[at] : phase[at]);
      worstOf[at] = size > worstOf[at] ? size : worstOf[at];
    }
  }
  if (!advanced) { std::printf("       %s\n", error.c_str()); }
  CHECK(advanced, "every frame of the steady-state run advances");
  if (!advanced || held.size() != (size_t)kRunFrames) { return Report(); }

  const size_t lap = (size_t)live->Frames();
  CHECK(lap > 1 && (size_t)kSettleFrames + lap < held.size(),
        "the run is longer than one lap of the subject's own grid past the settling point, so a pose "
        "can be compared with itself");
  if (lap <= 1 || (size_t)kSettleFrames + lap >= held.size()) { return Report(); }

  size_t pairs = 0, differing = 0;
  long long worst = 0, total = 0;
  for (size_t at = (size_t)kSettleFrames; at + lap < held.size(); ++at) {
    const long long moved = (long long)held[at + lap] - (long long)held[at];
    ++pairs;
    differing += moved != 0 ? 1u : 0u;
    total += moved;
    if (moved > worst) { worst = moved; }
  }
  const double perFrame = pairs > 0 ? (double)total / (double)pairs / (double)lap : 0.0;

  std::printf("NOTE the engine holds %zu bytes at the settling point and %zu at the end, over a lap "
              "of %zu frames\n",
              held[(size_t)kSettleFrames], held.back(), lap);
  std::printf("NOTE %zu pose-matched pairs one lap apart, %zu of them differing, worst %lld bytes\n",
              pairs, differing, worst);
  std::printf("NOTE the mean difference is %.4f bytes a frame\n", perFrame);
  std::printf("NOTE at %lld frames -- a hundred hours of suspend and resume at 60 Hz -- that is "
              "%.1f MB\n",
              kHorizonFrames, perFrame * (double)kHorizonFrames / (1024.0 * 1024.0));

  size_t moved = 0;
  for (size_t at = (size_t)kSettleFrames + 1; at < held.size(); ++at) {
    moved += held[at] != held[at - 1] ? 1u : 0u;
  }
  std::printf("NOTE frames after settling whose live bytes moved at all: %zu of %d -- a frame path "
              "that took nothing would read zero here\n",
              moved, kRunFrames - kSettleFrames - 1);

  for (size_t at = 0; at < outshine::Heap::TagCount(); ++at) {
    const char *tag = outshine::Heap::TagAt(at);
    if (tag == nullptr) { continue; }
    std::printf("NOTE taken under %-14s %zu bytes\n", tag, outshine::Heap::TakenAt(at));
  }
  static const char *const kPhase[4] = {"posing", "submitting", "aiming", "drawing"};
  for (int at = 0; at < 4; ++at) {
    std::printf("NOTE %-10s moved the heap on %4zu of %d frames, worst %zu bytes\n", kPhase[at],
                tookOn[at], kRunFrames - kSettleFrames, worstOf[at]);
  }

  CHECK(differing == 0,
        "the engine's own live bytes are the same number at the same pose one lap later -- a "
        "difference of one byte a frame is 21.6 MB over a hundred hours, so the frame path has to "
        "take nothing rather than take little (board:1463)");

  return Report();
}
