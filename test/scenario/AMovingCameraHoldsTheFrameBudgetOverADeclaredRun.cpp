/* **THE FOURTH CONSTRAINT BECOMING A MEASUREMENT** (board:1457). *720p60 on this device* has been a
 * sentence this repository quotes; here it is a distribution it publishes -- p50, p95 and p99 of frame
 * time over a camera that moves, taken on the surface the budget names.
 *
 * **THE CAMERA MOVES AND THE BODY DOES NOT, and that is the point rather than a simplification.** A
 * still subject under a moving eye is the case `Clients::Aim` was separated from `Clients::Show` for:
 * the geometry is set up once and the eye moves every frame. If a frame under a moving camera costs
 * what a frame under a still one costs, the separation is real; if it does not, something is being
 * rebuilt that nobody asked to rebuild.
 *
 * **THE DOMAIN IS DECLARED WITH THE NUMBER AND IT IS NOT THE SHIPPING FRAME.** Every frame here is
 * SERIALISED -- the device is waited on before the next one begins -- so no two frames are ever in
 * flight and nothing overlaps. That makes every figure below an **upper bound** on what this engine
 * would ship, never an estimate of it: a pipelined frame can only be faster. A number taken this way
 * that already fits the budget is a strong statement; one that does not would need the pipelined
 * measurement before it accused anything.
 *
 * **DETERMINISM IS TWO RUNS OF ONE DECLARATION, COMPARED PICTURE BY PICTURE.** A run that is not
 * reproducible cannot be regressed against, and every performance claim after it would be a sample of
 * a machine rather than a property of a declaration. */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
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

/* [SET] THE SURFACE THE BUDGET NAMES -- `CLAUDE.md`, 720p60 on this device. */
constexpr int kSurfaceW = 1280, kSurfaceH = 720;
/* [SET] 16.67 ms is the budget; a frame that misses it is seen by everyone. */
constexpr double kFrameBudgetMs = 16.67;
/* [SET] THE SHARE OF THE FRAME THE ENGINE'S OWN SIDE MAY TAKE. A tenth leaves nine for the device and
 * for whatever a game does that is not drawing; it is a number chosen on purpose and it is generous
 * against what is measured, which is what makes crossing it a finding rather than noise. */
constexpr double kEngineShare = 0.10;
/* **HOW MUCH OF THE FRAME THE SUBJECT MUST COVER** (board:1459). `ABeautifulGame` carries glass -- its
 * pawn tops -- so it is drawn by a plan with the transmissive pass in it, and that pass composited over
 * the opaque scene by ERASING it: the board, the pieces and their shadows were multiplied away and the
 * picture was sixteen pawn tops in the dark.
 *
 * [MEASURED] the defect covered **258 of 102 480** samples and the repair covers **9213**. The floor is
 * 4 % of the population -- less than half of what is measured, so ordinary variation in a moving camera
 * cannot trip it, and sixteen times what the defect left standing, so the defect cannot pass it. It is a
 * COVERAGE claim and never a picture one: what the pixels are worth is the render suite's. */
constexpr double kCoveredAtLeast = 0.04;

/* [SET] THE DECLARED RUN. Ninety frames at four degrees is a full turn and a half around the subject,
 * which is long enough for a distribution and short enough that a red run is diagnosable. */
constexpr int kRunFrames = 90;
/* **THE FRAMES THAT PAY FOR THE STAND-UP ARE NOT FRAMES OF THE RUN.** `Open` queues every pipeline,
 * every vertex and every image; the first advances are where the device finishes that work, and
 * [MEASURED] the largest of them is 659 ms against a p50 of 7.8. A steady-state budget is about steady
 * state, so they are excluded -- and the exclusion is DECLARED here rather than hidden in a percentile,
 * because a run that dropped its worst frames silently would be quoting a distribution it had cut. */
constexpr int kWarmupFrames = 10;
constexpr double kOrbitDegPerFrame = 4.0;
/* A DIAGNOSTIC ARM: the same run with a standing camera, so what the ORBIT costs is a difference of two
 * measurements rather than an attribution nobody took. */

constexpr double kFill = 0.9;
constexpr double kKeyLux = 3.0, kKeyElevationDeg = 35.0, kKeyBearingDeg = -35.0;
constexpr double kAmbient = 0.35;

/* THE SUBJECT, DECLARED. `ABeautifulGame` is the corpus's heaviest body -- a chess set of thirty-odd
 * pieces with its own textures -- which is what makes it worth putting under a moving camera: a run
 * over the lightest case would hold the budget and say nothing about whether the engine does. */
constexpr const char *kSubject = "ABeautifulGame";

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
  const char *standing = std::getenv("SCENARIO_STILL_CAMERA");
  if (standing != nullptr) { out.OrbitDegPerFrame = 0.0; }
  const char *named = std::getenv("SCENARIO_SUBJECT");
  out.Stands = EntryPath(outshine::Test::PreparedRoot() + "/" +
                         outshine::Test::kPreparedKhronosPrefix +
                         (named != nullptr ? named : kSubject));
  out.Fill = kFill;
  out.OrbitDegPerFrame = standing != nullptr ? 0.0 : kOrbitDegPerFrame;
  out.KeyLux = kKeyLux;
  out.KeyElevationDeg = kKeyElevationDeg;
  out.KeyBearingDeg = kKeyBearingDeg;
  out.Environment[0] = out.Environment[1] = out.Environment[2] = kAmbient;
  return out;
}

/* **THE POPULATION, DECLARED ONCE**, so a before and an after cannot select differently. */
constexpr int kStep = 3;

[[nodiscard]] size_t Population(void) {
  return (size_t)((kSurfaceH + kStep - 1) / kStep) * (size_t)((kSurfaceW + kStep - 1) / kStep);
}

[[nodiscard]] size_t Differing(const std::vector<uint8_t> &left, const std::vector<uint8_t> &right) {
  if (left.size() != right.size() || left.empty()) { return Population(); }
  size_t differ = 0;
  for (int y = 0; y < kSurfaceH; y += kStep) {
    for (int x = 0; x < kSurfaceW; x += kStep) {
      const size_t at = (((size_t)y * (size_t)kSurfaceW) + (size_t)x) * 4u;
      differ += left[at] != right[at] || left[at + 1] != right[at + 1] ||
                        left[at + 2] != right[at + 2]
                    ? 1u
                    : 0u;
    }
  }
  return differ;
}

struct Distribution {
  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0, MaxMs = 0.0;
  size_t Count = 0;
};

[[nodiscard]] Distribution Over(std::vector<double> &samples) {
  Distribution out;
  out.Count = samples.size();
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

/* ONE RUN OF THE DECLARATION: every frame advanced and WAITED ON, with the picture at `keepAt` read
 * back so two runs can be compared. The readback is outside the clock -- it is a stall this tree puts
 * in a test and never in a frame. */
[[nodiscard]] bool Run(outshine::Render::Renderer &renderer, int keepAt, std::vector<double> &into,
                       std::vector<double> &cpu, std::vector<uint8_t> &kept, std::string &error) {
  std::unique_ptr<outshine::Clients::Live> live;
  if (!outshine::Clients::Live::Open(renderer, Declared(), nullptr, live, error)) { return false; }
  std::printf("NOTE the scenario stood up with %u subject batches and %u draws\n",
              renderer.SubjectBatchCount(), renderer.SubjectDrawCount());
  into.clear();
  cpu.clear();
  into.reserve((size_t)kRunFrames);
  cpu.reserve((size_t)kRunFrames);
  for (int frame = 0; frame < kRunFrames; ++frame) {
    const auto began = std::chrono::steady_clock::now();
    const bool advanced = live->Advance(error);
    const auto handed = std::chrono::steady_clock::now();
    renderer.WaitForGpu();
    const auto ended = std::chrono::steady_clock::now();
    cpu.push_back(std::chrono::duration<double, std::milli>(handed - began).count());
    into.push_back(std::chrono::duration<double, std::milli>(ended - began).count());
    if (!advanced) { return false; }
    if (frame < kWarmupFrames) {
      into.pop_back();
      cpu.pop_back();
    }
    if (frame == keepAt && renderer.ReadPixels(kept) != outshine::Render::ReadState::Ready) {
      error = "the kept frame did not come off the device";
      return false;
    }
  }
  return true;
}

} // namespace

int main(void) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::string error;
  outshine::Render::Renderer renderer;

  std::printf("NOTE run = %d frames at %.1f deg per frame over %s, surface %dx%d\n", kRunFrames,
              kOrbitDegPerFrame, kSubject, kSurfaceW, kSurfaceH);
  std::printf("NOTE population = %zu samples on a %dx%d stride\n", Population(), kStep, kStep);
  std::printf("NOTE DOMAIN: every frame is serialised on the device, so these are UPPER BOUNDS on a "
              "shipping frame and never estimates of one\n");

  std::vector<double> first, second, firstCpu, secondCpu;
  std::vector<uint8_t> firstKept, secondKept;
  constexpr int kKeepAt = kRunFrames / 2;

  const bool ranOnce = Run(renderer, kKeepAt, first, firstCpu, firstKept, error);
  if (!ranOnce) { std::printf("       %s\n", error.c_str()); }
  CHECK(ranOnce, "the declared run stands up and every frame of it advances");
  if (!ranOnce) { return Report(); }

  const Distribution cost = Over(first);
  const Distribution cpu = Over(firstCpu);
  std::printf("NOTE serialised frame ms  p50 %.4f  p95 %.4f  p99 %.4f  max %.4f  over %zu frames\n",
              cost.P50Ms, cost.P95Ms, cost.P99Ms, cost.MaxMs, cost.Count);
  std::printf("NOTE the engine's own side ms  p50 %.4f  p95 %.4f  p99 %.4f  max %.4f\n", cpu.P50Ms,
              cpu.P95Ms, cpu.P99Ms, cpu.MaxMs);
  std::printf("NOTE budget %.2f ms -- the serialised p99 is %.1f%% of it, the engine's own p99 is "
              "%.2f%% of it\n",
              kFrameBudgetMs, 100.0 * cost.P99Ms / kFrameBudgetMs,
              100.0 * cpu.P99Ms / kFrameBudgetMs);

  /* **A SERIALISED FRAME IS AN UPPER BOUND ON A SHIPPING ONE, so holding the budget here is the
   * STRONGER statement.** Every frame waits on the device before the next begins, so CPU and GPU never
   * overlap and the figure is the sum of two things a shipping frame runs at once; a pipelined frame
   * can only be shorter. **The claim is therefore made in the direction the domain supports and in no
   * other**: passing decides that the budget holds, and failing would decide nothing until the same run
   * was measured with two frames in flight (board:1457).
   *
   * THE ENGINE'S OWN SIDE IS CHECKED SEPARATELY against a declared share, because it is the term this
   * repository can act on directly and the one a heavier device would not rescue. [MEASURED] it was
   * 1.06 ms p50 and 4.12 ms p99 until `board:1460`, and the whole of that was a per-vertex scan on the
   * frame path -- which is exactly the term this second number exists to expose. */
  CHECK(cost.P99Ms < kFrameBudgetMs,
        "the declared run holds the frame budget at p99 SERIALISED -- and a pipelined frame can only "
        "be shorter, so the bound holding decides that the budget holds");
  CHECK(cpu.P99Ms < kFrameBudgetMs * kEngineShare,
        "the engine's own side of a frame fits the share of the budget it was given, at p99 -- what "
        "the device then takes is a question this instrument cannot answer serialised");

  /* **THE CAMERA ACTUALLY MOVED.** A run that held the budget because nothing changed would be a
   * measurement of an idle device wearing the name of a frame. */
  std::vector<uint8_t> last;
  CHECK(renderer.ReadPixels(last) == outshine::Render::ReadState::Ready,
        "the last frame of the run comes off the device");
  /* A DIAGNOSTIC AND NOT A VERDICT: how much of the frame carries the body at all, so a small sweep
   * can be told from a small subject. */
  size_t ink = 0;
  for (int y = 0; y < kSurfaceH; y += kStep) {
    for (int x = 0; x < kSurfaceW; x += kStep) {
      const size_t at = (((size_t)y * (size_t)kSurfaceW) + (size_t)x) * 4u;
      ink += last[at] != 0 || last[at + 1] != 0 || last[at + 2] != 0 ? 1u : 0u;
    }
  }
  std::printf("NOTE samples carrying ink in the last frame: %zu of %zu\n", ink, Population());
  if (const char *where = std::getenv("SCENARIO_DUMP"); where != nullptr && !last.empty()) {
    if (std::FILE *out = std::fopen(where, "wb"); out != nullptr) {
      std::fprintf(out, "P6\n%d %d\n255\n", kSurfaceW, kSurfaceH);
      for (size_t at = 0; at + 3 < last.size(); at += 4) { std::fwrite(&last[at], 1, 3, out); }
      std::fclose(out);
    }
  }
  CHECK((double)ink >= kCoveredAtLeast * (double)Population(),
        "the subject covers the frame it was framed for -- a plan carrying the transmissive pass "
        "composites OVER the opaque scene and never erases it (board:1459)");
  const size_t swept = Differing(firstKept, last);
  std::printf("NOTE frame %d against the last frame: %zu of %zu samples differ\n", kKeepAt, swept,
              Population());
  CHECK(swept > 0,
        "the camera moved over the run -- a budget held by an unchanging picture measures an idle "
        "device and not a frame");

  /* **TWO RUNS OF ONE DECLARATION ARE THE SAME PICTURES.** Without this every number above is a sample
   * of a machine rather than a property of a declaration, and nothing later could be regressed. */
  const bool ranTwice = Run(renderer, kKeepAt, second, secondCpu, secondKept, error);
  if (!ranTwice) { std::printf("       %s\n", error.c_str()); }
  CHECK(ranTwice, "the same declaration stands up a second time");
  if (ranTwice) {
    const size_t drifted = Differing(firstKept, secondKept);
    std::printf("NOTE frame %d of run one against frame %d of run two: %zu of %zu samples differ\n",
                kKeepAt, kKeepAt, drifted, Population());
    CHECK(drifted == 0,
          "two runs of one declaration produce the same picture at the same frame -- the picture is a "
          "function of the declaration and not of the machine");
    const Distribution again = Over(second);
    const Distribution againCpu = Over(secondCpu);
    std::printf("NOTE second run serialised  p50 %.4f  p95 %.4f  p99 %.4f  max %.4f\n", again.P50Ms,
                again.P95Ms, again.P99Ms, again.MaxMs);
    std::printf("NOTE second run engine      p50 %.4f  p95 %.4f  p99 %.4f  max %.4f\n",
                againCpu.P50Ms, againCpu.P95Ms, againCpu.P99Ms, againCpu.MaxMs);
  }

  return Report();
}
