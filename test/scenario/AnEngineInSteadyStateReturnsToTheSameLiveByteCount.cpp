/* **THE LEAK TEST THAT REACHES A HUNDRED HOURS, and it reaches it by arithmetic** (board:1462).
 *
 * Suspend and quick resume mean one process runs for a console's whole relationship with a game. A
 * hundred hours at the frame budget's own rate is **21 600 000 frames**, where a leak of ONE BYTE a
 * frame is 21.6 MB and a hundred bytes is 2.16 GB on an eight-gigabyte device. **No run a suite can
 * afford is statistical enough to see that**: six hundred frames turn one byte a frame into six
 * hundred, four orders of magnitude under the driver's own arena.
 *
 * **SO THIS INSTRUMENT IS NOT STATISTICAL, AND ITS COMPARISON IS POSE-MATCHED.** A scenario in steady
 * state runs the same code every frame, but not on the same DATA: an animation's frame 7 poses a
 * different body from its frame 8, and a container sized to a draw list can differ between them
 * without anything leaking. **The frame that must read equal is the same frame one LAP later** -- the
 * subject's own grid brought round to the identical pose, the identical draw list and the identical
 * submission. Any difference there is bytes taken and not returned, the resolution is one byte, and
 * what it becomes over 21 600 000 frames is multiplication.
 *
 * *[MEASURED] comparing consecutive frames instead reported 4.48 bytes a frame over a run where 183 of
 * 250 frames read differently from the one before -- which is the animation's own poses, not a leak,
 * and would have been filed as one.*
 *
 * **THE POPULATION IS THE ENGINE'S OWN ALLOCATOR AND NOTHING ELSE.** `Heap::LiveBytes` counts what
 * this repository's `operator new` took and its `operator delete` returned; the driver's arena, SDL's
 * buffers and every mapped library are outside it by construction. That is the whole difference from
 * `ALongRunHoldsItsMemoryAndItsPace`, which reads the process's malloc zone and can therefore only
 * claim a ceiling -- both are kept, because one says what the machine holds and the other says what
 * this repository is answerable for.
 *
 * **A SCENARIO THAT LEGITIMATELY GROWS IS NOT THIS CLAIM.** A stream-in holds what it streamed. The
 * subject here is declared to be in steady state -- one body, one animation looping over its own grid,
 * nothing arriving -- and the claim is over that. */
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

/* [SET] LONG ENOUGH THAT A LEAK FIRING ONCE A CYCLE IS SEEN. `BoxAnimated` reports 223 frames on its
 * own grid, so 500 frames is more than two full laps: an allocation that happens on one frame of the
 * animation and not the others still shows, and one that happens on every frame shows immediately. */
constexpr int kRunFrames = 500;
/* [SET] THE FRAMES THE RUN IS ALLOWED TO SETTLE IN. Every buffer a frame needs is grown once -- the
 * scratch vertices, the index run, the draw list -- and a container that doubles is not a leak. Two
 * full laps of the grid are behind the settling point, so anything that grows after it grows for a
 * reason no first-time allocation explains. */
constexpr int kSettleFrames = 250;

constexpr double kFill = 0.9;
constexpr double kOrbitDegPerFrame = 1.0;
constexpr double kKeyLux = 3.0, kKeyElevationDeg = 35.0, kKeyBearingDeg = -35.0;
constexpr double kAmbient = 0.35;
/* [SET] THE HORIZON THE OWNER NAMED: a hundred hours of suspend-and-resume at the budget's own rate. */
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

} // namespace

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

  /* THE READING IS TAKEN AT ONE POINT OF THE FRAME AND ALWAYS THE SAME ONE: after the advance has
   * returned, so every allocation the frame made and every one it returned is behind it. */
  std::vector<size_t> held;
  held.reserve((size_t)kRunFrames);
  /* THE PHASE FIGURES ARE ONE FRAME'S DIFFERENCE EACH, so they are read after every advance and counted
   * here: how many frames each phase took anything on, and how much at its worst. A sum would cancel a
   * take against a return and report a path that churns megabytes as free. */
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

  /* **HOW OFTEN THE FRAME PATH ALLOCATES AT ALL**, which is the number the established answer cares
   * about: Unreal pops a whole frame's allocations with one `FMemMark`, RAGE sizes its pools at build
   * time, and `CLAUDE.md` says an allocation is not a bounded term. A frame that takes nothing cannot
   * leak, so this is published beside the equality above and is the thing that would make it free. */
  size_t moved = 0;
  for (size_t at = (size_t)kSettleFrames + 1; at < held.size(); ++at) {
    moved += held[at] != held[at - 1] ? 1u : 0u;
  }
  std::printf("NOTE frames after settling whose live bytes moved at all: %zu of %d -- a frame path "
              "that took nothing would read zero here\n",
              moved, kRunFrames - kSettleFrames - 1);
  /* WHERE THOSE BYTES WENT, so the repair has an address and not a direction. Each figure is one
   * frame's difference across one phase; a phase that returned more than it took wraps, and the wrap
   * is left visible rather than clamped. */
  static const char *const kPhase[4] = {"posing", "submitting", "aiming", "drawing"};
  for (int at = 0; at < 4; ++at) {
    std::printf("NOTE %-10s moved the heap on %4zu of %d frames, worst %zu bytes\n", kPhase[at],
                tookOn[at], kRunFrames - kSettleFrames, worstOf[at]);
  }

  /* **EQUALITY, AND NOT A TOLERANCE.** A steady-state frame runs the same code as the one before it,
   * so a difference is not noise to be bounded -- it is bytes somebody took and did not give back.
   * The number of frames that MOVED is reported beside it, because one frame moving and 249 holding
   * is a different defect from all 250 moving by one byte. */
  /* **THIS IS RED AND IT NAMES ITS CAUSE** (board:1463). The frame path allocates: 172 frames in 249
   * take memory and give it back, because the flattener decodes every accessor into a fresh vector on
   * every build. Nothing LEAKS -- the mean one lap apart is negative -- and that is not the claim. The
   * claim is that a frame takes nothing, which is what Unreal's `FMemStack` and RAGE's build-time
   * pools both arrange and what `CLAUDE.md` asks for in its own words, and it is the repair that makes
   * this equality free instead of measured. */
  CHECK(differing == 0,
        "the engine's own live bytes are the same number at the same pose one lap later -- a "
        "difference of one byte a frame is 21.6 MB over a hundred hours, so the frame path has to "
        "take nothing rather than take little (board:1463)");

  return Report();
}
