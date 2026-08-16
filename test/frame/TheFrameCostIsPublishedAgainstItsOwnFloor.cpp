/* THE FRAME COST OF THIS SOURCE, PUBLISHED WITH THE FLOOR THAT SAYS WHAT A DIFFERENCE IS WORTH
 * (board:1187).
 *
 * THE GAP THIS CLOSES IS A COMPARISON AND NOT AN INSTRUMENT. The clock, the moving camera, the timed
 * frames and the sanitiser-free path were all here; what was missing is the previous run's numbers,
 * so no change in this engine had ever been priced -- four feature rows landed and the cost of all
 * four together was unknown.
 *
 * A STORED BASELINE IN THE TREE IS THE WRONG ANSWER AND IS NOT WHAT THIS DOES. A committed number is
 * a second place for the truth to live and a threshold that moves without a diff. This run PUBLISHES
 * its distribution, keyed by the digest of the sources it was built from, into a directory outside
 * the tree; a later run finds what earlier runs left there and PRINTS the comparison. Nothing here
 * updates a number, and no verdict is taken from a previous run -- the comparison is a reader's.
 *
 * THE FLOOR COMES FIRST, because until two runs of one unchanged binary have been shown to differ by
 * some amount, no difference between two commits can be called a regression. Each arm is timed
 * `kRepeats` times over, arms interleaved so a drifting host spreads across all of them rather than
 * landing on one, and the spread of an arm's p50 across those repeats is the width below which this
 * instrument resolves nothing. Two runs of one binary leave two records under one digest, and the
 * comparison then prints the ACROSS-RUN floor as well, which is the one a commit-to-commit
 * comparison is actually made at.
 *
 * IT PRICES A KNOWN-SIGN CHANGE EVERY TIME IT RUNS, and that is the gate. `fill` and `fill-twice-lit`
 * are one subject on one path differing by exactly one shadow ray per surviving fragment, so the
 * second must cost more than the first by more than the two floors together -- a baseline nobody has
 * ever compared against is an unexercised capability, and this is the comparison that cannot rot.
 *
 * THE ARMS ARE FIXED AND DECLARED, NEVER DERIVED. The sibling instrument searches for the distance a
 * subject fills the frame at; a search would put a different population under two runs of the same
 * arm the moment the picture changed, and a number whose population moved underneath it reads as
 * progress. The filling scales here are the ones that search PUBLISHED, written down as declarations.
 *
 * IT IS NOT A CASE DIRECTORY AND HAS NO ORACLE. Whether the picture is right is the render suite's
 * business. What this gates is the two things a timing run can be wrong about without saying so:
 * that every timed frame drew the subject, and that every repeat of an arm drew the same picture. */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "Check.h"

#include "Orbit.h"
#include "SourceDigest.h"
#include "WhatIsDrawn.h"

#include "Document.h"
#include "GltfStudio.h"
#include "Image.h"
#include "RenderPlan.h"
#include "Renderer.h"
#include "Subject.h"

namespace {

/* THE FRAME THE FOURTH CONSTRAINT IS STATED AT (CLAUDE.md): 720p, and the budget that goes with it
 * is one sixtieth of a second. */
constexpr int kFrameWidthPx = 1280;
constexpr int kFrameHeightPx = 720;
constexpr double kFrameBudgetMs = 1000.0 / 60.0;

/* HOW MANY FRAMES ONE REPEAT OF ONE ARM IS TIMED OVER. [SET] 240, which is the sibling
 * instrument's count: four seconds of a 60 Hz camera path, two frames past the 99th percentile so
 * the p99 is a measured order statistic. It is the same number as the sibling's on purpose -- two
 * instruments whose order statistics are taken over different lengths publish p99s that are not
 * comparable, and both publish into one archive. */
constexpr int kTimedFrames = 240;
/* Frames drawn and thrown away before the clock starts, so the first-use cost of a pipeline, a
 * buffer and a texture is not counted as a frame's. */
constexpr int kWarmFrames = 20;
/* HOW MANY POINTS OF THE PATH THE PICTURE IS SAMPLED AT. [SET] 8, the sibling's count, for the same
 * reason the frame count is the sibling's. */
constexpr int kProbes = 8;
/* HOW MANY TIMES EACH ARM IS MEASURED, AND IT IS THE FLOOR'S SAMPLE SIZE. [SET] 5: a spread over a
 * pair is one difference and says nothing about how a third repeat would land, and the whole run has
 * to fit inside the harness's timeout with four arms in it. */
constexpr int kRepeats = 5;

/* WHETHER AN ARM BINDS THE FILE'S SOCKETS OR DRAWS A DECLARED CONSTANT. An enumeration and not a
 * bool because the arm table has to say which it is at each row (`Enum.2`), and because a third
 * answer -- one socket rather than four -- is a change of this word and not of a call site. */
enum class Appearance { Flat, Textured };

/* ONE MEASURED POPULATION: which subject, seen from where, under how many lights, wearing what. Every
 * field of it is published on the arm's line, because a frame number without its population is the
 * same defect one domain over. */
struct Arm {
  const char *Id;
  const char *Subject;
  const char *Path;
  /* WHERE ON ITS OWN FRAMING ORBIT THE SUBJECT IS SEEN FROM, declared and never searched for.
   * 1.00 is the distance the file itself derives. 0.25 is [SET] from a sweep of this instrument's own
   * path over 0.50, 0.35, 0.25, 0.18 and 0.12: it is the closest standpoint whose coverage is still
   * steady along the path, and the arm line publishes the extremes it is steady between.
   *
   * THE SIBLING INSTRUMENT SEARCHES FOR THE COVERAGE MAXIMUM INSTEAD, and that is refuted here rather
   * than repeated: it lands on 0.12, where the median coverage is the whole frame and the cheapest
   * frame of the path costs 0.389 ms against a p50 of 2.031 -- a factor of five inside one arm, which
   * is two populations under one name. A maximum is not a standpoint. */
  double Scale;
  int Lights;
  Appearance Skin;
};

/* THE FOUR ARMS, AND EACH ONE IS A DIFFERENT THING A CHANGE COULD MOVE.
 *
 * `geometry` is 1.5 M triangles covering 3 % of the frame with nothing lighting it, so its cost is
 * vertex work and the acceleration structure and almost no shading. `fill` is a subject at the
 * distance where it covers every pixel of the frame under one light, so its cost is the fragment.
 * `fill-twice-lit` is `fill` with one more ray per surviving fragment and nothing else changed --
 * the known-sign difference this instrument is gated on. `texture` is `fill` with the four image
 * sockets bound, so a change to the tap path lands in it and in nothing else.
 *
 * THE TWO SUBJECTS ARE THE CORPUS'S OWN, not a stand-in: traversal and fill both depend on how the
 * triangles are distributed and not only on how many there are. */
constexpr Arm kArms[] = {
    {"geometry", "a-beautiful-game", "test/render/materials/a-beautiful-game/scene.gltf", 1.00, 0,
     Appearance::Flat},
    {"fill", "scifi-helmet", "test/render/materials/scifi-helmet/scene.gltf", 0.25, 1,
     Appearance::Flat},
    {"fill-twice-lit", "scifi-helmet", "test/render/materials/scifi-helmet/scene.gltf", 0.25, 2,
     Appearance::Flat},
    {"texture", "scifi-helmet", "test/render/materials/scifi-helmet/scene.gltf", 0.25, 1,
     Appearance::Textured},
};

constexpr size_t kArmCount = sizeof(kArms) / sizeof(kArms[0]);

/* WHICH ROW OF THE TABLE AN ARM'S NAME IS. The gate below names the two arms it compares rather than
 * indexing them, because a row inserted above them would otherwise silently change what is priced --
 * and the failure would read as a measurement rather than as a mistake. */
[[nodiscard]] constexpr size_t ArmNamed(std::string_view id) {
  for (size_t at = 0; at < kArmCount; ++at) {
    if (id == kArms[at].Id) { return at; }
  }
  return kArmCount;
}
static_assert(ArmNamed("fill") < kArmCount && ArmNamed("fill-twice-lit") < kArmCount,
              "the two arms the known-sign gate is taken over are in the table");

/* THE SIDE OF THE GENERATED RASTER EVERY BOUND SOCKET READS. [SET] 512 texels: large enough that a
 * frame-filling subject minifies rather than magnifies it, so the sampler does the work a real
 * material's would, and small enough that four of them are a megabyte and a half of upload. */
constexpr uint32_t kRasterSide = 512;

/* ONE ARM'S FRAME TIMES, SORTED, and the order statistics read off them. A mean is not among them
 * and that is the rule rather than a preference (CLAUDE.md). */
struct Distribution {
  double P50Ms = 0.0;
  double P95Ms = 0.0;
  double P99Ms = 0.0;
  double MinMs = 0.0;
  double MaxMs = 0.0;
};

[[nodiscard]] Distribution Over(std::vector<double> &samples) {
  Distribution out;
  if (samples.empty()) { return out; }
  std::sort(samples.begin(), samples.end());
  const auto at = [&samples](double fraction) {
    const size_t index = (size_t)(fraction * (double)(samples.size() - 1) + 0.5);
    return samples[index];
  };
  out.P50Ms = at(0.50);
  out.P95Ms = at(0.95);
  out.P99Ms = at(0.99);
  out.MinMs = samples.front();
  out.MaxMs = samples.back();
  return out;
}

/* THE APPEARANCE IS GENERATED HERE AND NOT FETCHED (CLAUDE.md): the question is what a tap costs,
 * and a tap costs what it costs whatever the texels say -- but the texels still have to VARY, or a
 * constant image is a cache line the sampler never leaves. The pattern is a product of two sines
 * plus a checker, which is band-limited, recomputable, and different in every socket because the
 * four are offset against each other. */
std::vector<uint8_t> GeneratedRaster(uint32_t side, int socket) {
  std::vector<uint8_t> rgba((size_t)side * side * 4u);
  for (uint32_t y = 0; y < side; ++y) {
    for (uint32_t x = 0; x < side; ++x) {
      const double u = (double)x / (double)side;
      const double v = (double)y / (double)side;
      const double wave = 0.5 + 0.5 * std::sin((u * 9.0 + (double)socket * 0.7) * 6.2831853) *
                                    std::sin((v * 7.0 + (double)socket * 0.3) * 6.2831853);
      const bool cell = (((x * 8u) / side) + ((y * 8u) / side)) % 2u == 0u;
      const size_t at = ((size_t)y * side + x) * 4u;
      rgba[at + 0] = (uint8_t)(255.0 * (cell ? wave : 1.0 - wave));
      rgba[at + 1] = (uint8_t)(255.0 * wave);
      rgba[at + 2] = (uint8_t)(255.0 * (0.5 + 0.5 * wave));
      rgba[at + 3] = 255;
    }
  }
  return rgba;
}

/* THE FOUR RASTERS AN ARM'S SURFACE POINTS AT, OWNED FOR AS LONG AS THE SURFACE IS. `SubjectTexture`
 * holds a borrowed pointer (`R.3`), so the bytes have to outlive the `Show` that uploads them, and a
 * vector built inside the setup function would not (`F.43`). */
struct BoundRasters {
  std::vector<uint8_t> Colour;
  std::vector<uint8_t> Normal;
  std::vector<uint8_t> MetalRough;
  std::vector<uint8_t> Emissive;

  BoundRasters()
      : Colour(GeneratedRaster(kRasterSide, 0)), Normal(GeneratedRaster(kRasterSide, 1)),
        MetalRough(GeneratedRaster(kRasterSide, 2)), Emissive(GeneratedRaster(kRasterSide, 3)) {}
};

void BindSocket(const std::vector<uint8_t> &texels, outshine::Render::SubjectTexture &socket) {
  socket.Rgba = texels.data();
  socket.Width = kRasterSide;
  socket.Height = kRasterSide;
}

/* THE SUBJECT AS THIS INSTRUMENT TAKES IT: one surface, mid-grey, half rough, and either no image at
 * all or all four sockets bound. The appearance is deliberately not the file's -- a subject whose
 * material table the reader resolved would make an arm's cost a property of which asset was checked
 * out, and this instrument's arms have to mean the same thing in every run. */
outshine::Clients::Studio StudioOver(const outshine::Gltf::Subject &subject, Appearance skin,
                                     const BoundRasters &rasters) {
  outshine::Clients::Studio studio;
  studio.Geometry = &subject;
  outshine::Render::SubjectMaterial surface;
  surface.Row.BaseColour[0] = surface.Row.BaseColour[1] = surface.Row.BaseColour[2] = 0.5f;
  surface.Row.BaseColour[3] = 1.0f;
  surface.Row.Roughness = 0.5f;
  surface.Row.Metalness = 0.0f;
  if (skin == Appearance::Textured) {
    BindSocket(rasters.Colour, surface.Colour);
    BindSocket(rasters.Normal, surface.Normal);
    BindSocket(rasters.MetalRough, surface.MetalRough);
    BindSocket(rasters.Emissive, surface.Emissive);
  }
  studio.Surfaces.push_back(surface);
  studio.EmittedRadiance.assign(subject.Parts().size(), {0.0f, 0.0f, 0.0f});
  studio.PartSurface.assign(subject.Parts().size(), 0u);
  return studio;
}

/* THE LIGHTS ONE ARM DECLARES: `count` suns pointing in different directions, all delta, all
 * reaching the subject. They are spread rather than stacked because two lights from one direction
 * would trace two rays down one path and the second would be free in the cache -- which would price
 * the second ray at less than a ray costs, and the second ray is what this instrument is gated on. */
std::vector<outshine::PunctualLight> SunsFacingEveryWay(int count) {
  std::vector<outshine::PunctualLight> lights;
  for (int at = 0; at < count; ++at) {
    const double turn = 2.0 * 3.14159265358979323846 * (double)at / (double)std::max(count, 1);
    outshine::PunctualLight light;
    light.Kind = outshine::LightKind::Directional;
    light.Intensity = 3.14159265358979323846f;
    light.Colour[0] = light.Colour[1] = light.Colour[2] = 1.0f;
    light.Direction[0] = (float)(0.5 * std::cos(turn));
    light.Direction[1] = -0.7f;
    light.Direction[2] = (float)(0.5 * std::sin(turn));
    const float length = std::sqrt(light.Direction[0] * light.Direction[0] +
                                   light.Direction[1] * light.Direction[1] +
                                   light.Direction[2] * light.Direction[2]);
    for (int axis = 0; axis < 3; ++axis) { light.Direction[axis] /= length; }
    lights.push_back(light);
  }
  return lights;
}

/* EVERYTHING ONE ARM PRODUCED OVER THE WHOLE RUN: one distribution per repeat, and the picture the
 * repeats drew. The floor is read off the repeats and is not a field, because it is derived from
 * them and a stored copy is a second place for it to live. */
struct Measured {
  std::vector<Distribution> Repeats;
  long CoveredPx = 0;
  long LeastCoveredPx = 0;
  long MostCoveredPx = 0;
  double SumRadiance = 0.0;
  bool EveryRepeatDrewTheSamePicture = true;
  bool Rendered = true;
  std::string Refusal;

  /* ONE ORDER STATISTIC OF EVERY REPEAT, SORTED. `which` names the member rather than an index,
   * because a p95 read out of the slot a p50 was meant to be in is a wrong number that reads
   * right. */
  [[nodiscard]] std::vector<double> Across(double Distribution::*which) const {
    std::vector<double> out;
    for (const Distribution &repeat : Repeats) { out.push_back(repeat.*which); }
    std::sort(out.begin(), out.end());
    return out;
  }
  /* THE REPRESENTATIVE STATISTIC OF THE ARM: the median of the repeats', so one repeat that landed
   * on a busy host does not become the arm's number. */
  [[nodiscard]] double MedianOf(double Distribution::*which) const {
    const std::vector<double> sorted = Across(which);
    return sorted.empty() ? 0.0 : sorted[sorted.size() / 2];
  }
  [[nodiscard]] double P50Ms() const { return MedianOf(&Distribution::P50Ms); }
  [[nodiscard]] double P95Ms() const { return MedianOf(&Distribution::P95Ms); }
  [[nodiscard]] double P99Ms() const { return MedianOf(&Distribution::P99Ms); }
  /* THE INSTRUMENT'S OWN WIDTH ON THIS ARM: how far apart the extreme repeats of ONE UNCHANGED
   * BINARY landed. A difference smaller than this is not a difference. */
  [[nodiscard]] double FloorMs() const {
    const std::vector<double> sorted = Across(&Distribution::P50Ms);
    return sorted.empty() ? 0.0 : sorted.back() - sorted.front();
  }
};

/* ONE REPEAT OF ONE ARM, SET UP FROM SCRATCH AND TIMED. The studio is rebuilt every repeat on
 * purpose: a repeat that reused the standing mesh would measure the same residency five times and
 * report a floor narrower than the one a second RUN will actually meet. */
[[nodiscard]] bool TimeOnce(outshine::Render::Renderer &renderer,
                            const outshine::Gltf::Subject &subject,
                            const outshine::Gltf::Placement &framed, const Arm &arm,
                            const BoundRasters &rasters, Distribution &out, double &setupMs,
                            std::string &error) {
  outshine::Clients::Studio studio = StudioOver(subject, arm.Skin, rasters);
  studio.Lights = SunsFacingEveryWay(arm.Lights);
  outshine::Clients::StudioScratch scratch;

  studio.Eye = outshine::Test::OrbitAt(subject, framed, arm.Scale, 0, kTimedFrames);
  const auto setupBegan = std::chrono::steady_clock::now();
  if (!outshine::Clients::Show(renderer, studio, scratch, error)) { return false; }
  renderer.WaitForGpu();
  setupMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - setupBegan)
                .count();

  for (int warm = 0; warm < kWarmFrames; ++warm) {
    if (!outshine::Clients::Aim(
            renderer, subject,
            outshine::Test::OrbitAt(subject, framed, arm.Scale, warm, kTimedFrames), error)) {
      return false;
    }
    renderer.RenderFrame();
  }
  renderer.WaitForGpu();

  std::vector<double> samples;
  samples.reserve(kTimedFrames);
  for (int step = 0; step < kTimedFrames; ++step) {
    if (!outshine::Clients::Aim(
            renderer, subject,
            outshine::Test::OrbitAt(subject, framed, arm.Scale, step, kTimedFrames), error)) {
      return false;
    }
    const auto began = std::chrono::steady_clock::now();
    renderer.RenderFrame();
    renderer.WaitForGpu();
    const auto ended = std::chrono::steady_clock::now();
    samples.push_back(std::chrono::duration<double, std::milli>(ended - began).count());
  }
  out = Over(samples);
  return true;
}

/* WHERE A RUN LEAVES ITS RECORD AND WHERE IT LOOKS FOR EARLIER ONES. Outside the tree, because a
 * committed measurement is a second place for the truth to live and a number that moves without a
 * diff (CLAUDE.md); under a name of this instrument's own, because the harness's build directory is
 * the harness's statement and not this one's. */
std::filesystem::path ArchiveDirectory(void) {
  const char *temp = std::getenv("TMPDIR");
  return std::filesystem::path(temp && *temp ? temp : "/tmp") / "outshine-frame";
}

/* ONE FRAME OF EACH ARM, WRITTEN WHERE A READER CAN OPEN IT. A timing instrument whose arms nobody
 * ever looks at can be timing a subject drawn wrong, drawn black or drawn as a silhouette, and every
 * number it publishes would still be a number: coverage above zero is not a picture. It is taken
 * outside every timed span, once per run, at STEP ZERO of the path so that two runs put the same
 * viewpoint side by side, into the archive directory beside the record -- never into
 * the tree, because a file nobody committed on purpose is a file the next round has to decide
 * about. */
void WriteStill(outshine::Render::Renderer &renderer, const outshine::Gltf::Subject &subject,
                const outshine::Gltf::Placement &framed, double scale,
                const std::filesystem::path &to) {
  std::string error;
  if (!outshine::Clients::Aim(renderer, subject,
                              outshine::Test::OrbitAt(subject, framed, scale, 0, kTimedFrames),
                              error)) {
    return;
  }
  renderer.RenderFrame();
  std::vector<uint8_t> rgba;
  if (renderer.ReadPixels(rgba) != outshine::Render::ReadState::Ready) { return; }
  std::vector<uint8_t> png;
  if (!outshine::Clients::EncodePng(rgba.data(), kFrameWidthPx, kFrameHeightPx, png)) { return; }
  std::ofstream file(to, std::ios::binary);
  file.write((const char *)png.data(), (std::streamsize)png.size());
}

/* ONE PUBLISHED MEASUREMENT AS A LATER RUN READS IT BACK. It is whitespace-separated text with the
 * digest first, so the archive is greppable by source identity with no parser anywhere. */
struct Record {
  std::string Digest;
  long long Ran = 0;
  std::string Arm;
  double P50Ms = 0.0;
  double P95Ms = 0.0;
  double P99Ms = 0.0;
  double FloorMs = 0.0;
};

std::vector<Record> ArchivedBefore(void) {
  std::vector<Record> out;
  std::error_code failed;
  for (std::filesystem::directory_iterator at(ArchiveDirectory(), failed), end; at != end;
       at.increment(failed)) {
    if (failed) { return out; }
    if (at->path().extension() != ".txt") { continue; }
    std::ifstream file(at->path());
    Record row;
    while (file >> row.Digest >> row.Ran >> row.Arm >> row.P50Ms >> row.P95Ms >> row.P99Ms >>
           row.FloorMs) {
      out.push_back(row);
    }
  }
  return out;
}

/* WHEN THE RUNNING BINARY WAS LINKED, on the same clock the source population is read with, so the
 * two are comparable to each other and to nothing else. */
long long WhenBuilt(const char *binary) {
  std::error_code failed;
  const auto written = std::filesystem::last_write_time(binary, failed);
  if (failed) { return 0; }
  return (long long)std::chrono::duration_cast<std::chrono::seconds>(written.time_since_epoch())
      .count();
}

/* WHAT EVERY ARM DRAWS, READ ONCE AND HELD FOR THE WHOLE RUN. The three runs are parallel and are
 * one object because they are one thing (`C.1`): a `Subject` points into its `Document`, so a
 * document that outlived its subject by one line would be a dangling read, and a framing belongs to
 * the subject it was derived from. One entry per ARM and not per file -- two arms over one file are
 * two entries, which costs a second read and buys that no arm can be handed another's placement. */
struct Standing {
  std::vector<outshine::Gltf::Document> Documents;
  std::vector<outshine::Gltf::Subject> Subjects;
  std::vector<outshine::Gltf::Placement> Framings;
};

[[nodiscard]] bool ReadEveryArmsSubject(Standing &out) {
  out.Documents.resize(kArmCount);
  out.Subjects.resize(kArmCount);
  out.Framings.resize(kArmCount);
  bool everyArmStands = true;
  for (size_t at = 0; at < kArmCount; ++at) {
    if (!out.Documents[at].ReadFile(kArms[at].Path)) {
      std::printf("ARM %s unread: %s\n", kArms[at].Id, out.Documents[at].Error().c_str());
      everyArmStands = false;
      continue;
    }
    if (!out.Subjects[at].Build(out.Documents[at]) || !out.Subjects[at].Frame(out.Framings[at])) {
      std::printf("ARM %s unbuilt: %s\n", kArms[at].Id, out.Subjects[at].Error().c_str());
      everyArmStands = false;
    }
  }
  if (!everyArmStands) { out.Subjects.clear(); }
  return everyArmStands;
}

/* EVERY ARM, EVERY REPEAT, IN THE ONE ORDER THAT KEEPS THEM COMPARABLE. The arms are interleaved
 * INSIDE the repeat and not the other way round: a host that warms over the run would otherwise put
 * all of one arm's repeats in the cool part and all of another's in the warm part, and the drift
 * would be read as an arm difference. */
void MeasureEveryArm(outshine::Render::Renderer &renderer, const Standing &standing,
                     const BoundRasters &rasters, std::vector<Measured> &measured) {
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    for (size_t at = 0; at < kArmCount; ++at) {
      const Arm &arm = kArms[at];
      Distribution spread;
      double setupMs = 0.0;
      std::string error;
      if (!TimeOnce(renderer, standing.Subjects[at], standing.Framings[at], arm, rasters, spread,
                    setupMs, error)) {
        measured[at].Rendered = false;
        measured[at].Refusal = error;
        continue;
      }
      measured[at].Repeats.push_back(spread);
      const outshine::Test::Drawn drawn = outshine::Test::WhatThePathDraws(
          renderer, standing.Subjects[at], standing.Framings[at], arm.Scale, kTimedFrames, kProbes);
      if (repeat == 0) {
        measured[at].CoveredPx = drawn.MedianCoveredPx;
        measured[at].LeastCoveredPx = drawn.LeastCoveredPx;
        measured[at].MostCoveredPx = drawn.MostCoveredPx;
        measured[at].SumRadiance = drawn.SumRadiance;
        std::error_code ignored;
        std::filesystem::create_directories(ArchiveDirectory(), ignored);
        WriteStill(renderer, standing.Subjects[at], standing.Framings[at], arm.Scale,
                   ArchiveDirectory() / (std::string(arm.Id) + ".png"));
      } else if (drawn.MedianCoveredPx != measured[at].CoveredPx ||
                 drawn.SumRadiance != measured[at].SumRadiance) {
        measured[at].EveryRepeatDrewTheSamePicture = false;
      }
      std::printf("REPEAT %s %d setup=%.1f ms p50=%.3f p95=%.3f p99=%.3f min=%.3f max=%.3f\n",
                  arm.Id, repeat, setupMs, spread.P50Ms, spread.P95Ms, spread.P99Ms, spread.MinMs,
                  spread.MaxMs);
    }
  }
}

/* ONE LINE PER ARM CARRYING ITS WHOLE POPULATION, and the two things a timing run can be wrong about
 * without saying so: that it drew the subject at all, and that its repeats drew one picture. */
void PublishEveryArm(const Standing &standing, const std::vector<Measured> &measured) {
  for (size_t at = 0; at < kArmCount; ++at) {
    const Arm &arm = kArms[at];
    const Measured &result = measured[at];
    CHECK(result.Rendered, "every declared arm rendered");
    if (!result.Rendered) {
      std::printf("ARM %s refused: %s\n", arm.Id, result.Refusal.c_str());
      continue;
    }
    std::printf("ARM %s subject=%s tris=%zu scale=%.2f lights=%d skin=%s covered=%ld px of %d "
                "(%ld..%ld over the path) frames=%d repeats=%d p50=%.3f p95=%.3f p99=%.3f "
                "floor=%.3f ms (%.1f%%) budget=%.1f%%\n",
                arm.Id, arm.Subject, standing.Subjects[at].TriangleCount(), arm.Scale, arm.Lights,
                arm.Skin == Appearance::Textured ? "textured" : "flat", result.CoveredPx,
                kFrameWidthPx * kFrameHeightPx, result.LeastCoveredPx, result.MostCoveredPx,
                kTimedFrames, kRepeats, result.P50Ms(), result.P95Ms(), result.P99Ms(),
                result.FloorMs(), 100.0 * result.FloorMs() / result.P50Ms(),
                100.0 * result.P50Ms() / kFrameBudgetMs);
    CHECK(result.CoveredPx > 0, "the timed frames drew the subject rather than an empty target");
    CHECK(result.EveryRepeatDrewTheSamePicture,
          "every repeat of an arm drew the same picture, so the repeats are of one thing");
  }
}

/* WHAT EARLIER RUNS LEFT, COMPARED AND PRINTED AND NEVER JUDGED. A record under this run's own
 * digest is a second run of one unchanged source, so its delta is the ACROSS-RUN floor -- the one a
 * commit-to-commit comparison is actually made at, and always wider than the within-run one. A record
 * under another digest is a priced change, and whether it is a regression is a reader's sentence and
 * not this test's verdict. */
void CompareWithEarlierRuns(const std::vector<Record> &earlier,
                            const std::vector<Measured> &measured, const std::string &digest) {
  std::printf("ARCHIVE %s holds %zu earlier measurement(s)\n", ArchiveDirectory().c_str(),
              earlier.size());
  for (const Record &before : earlier) {
    for (size_t at = 0; at < kArmCount; ++at) {
      if (before.Arm != kArms[at].Id || !measured[at].Rendered || before.P50Ms <= 0.0) { continue; }
      const double now = measured[at].P50Ms();
      const double delta = now - before.P50Ms;
      const double width = measured[at].FloorMs() + before.FloorMs;
      /* `archived` AND `thisrun`, NEVER `was` AND `now`. The archive carries no order of COMMITS --
       * a run of an older source can land in it after a run of a newer one, which is exactly what
       * happened the first time this was read -- so a word implying chronology would say something
       * the record cannot support. The digest is what identifies the other side. */
      std::printf("COMPARE %s arm=%s archived=%.3f thisrun=%.3f delta=%+.3f ms (%+.1f%%) "
                  "floor-sum=%.3f ms %s archived-digest=%s archived-at=%lld\n",
                  before.Digest == digest ? "SAME-SOURCE" : "CHANGED-SOURCE", kArms[at].Id,
                  before.P50Ms, now, delta, 100.0 * delta / before.P50Ms, width,
                  std::fabs(delta) > width ? "RESOLVED" : "within-floor", before.Digest.c_str(),
                  before.Ran);
    }
  }
}

/* THIS RUN, APPENDED UNDER ITS SOURCE'S KEY. Appended and never rewritten: two runs of one source
 * are the across-run floor, and a file that held only the latest would throw that away. */
void Archive(const std::string &digest, const std::vector<Measured> &measured) {
  std::error_code ignored;
  std::filesystem::create_directories(ArchiveDirectory(), ignored);
  std::ofstream archive(ArchiveDirectory() / (digest + ".txt"), std::ios::app);
  const long long ran = (long long)std::time(nullptr);
  for (size_t at = 0; at < kArmCount; ++at) {
    if (!measured[at].Rendered) { continue; }
    const Measured &result = measured[at];
    archive << digest << ' ' << ran << ' ' << kArms[at].Id << ' ' << result.P50Ms() << ' '
            << result.P95Ms() << ' ' << result.P99Ms() << ' ' << result.FloorMs() << '\n';
  }
}

} // namespace

/* `argv[0]` IS THE ONLY WAY A PROCESS LEARNS WHICH BINARY IT IS, and that is why this one takes
 * arguments where the rest of the layer takes none (`F.9`: the count is unused and so unnamed). */
int main(int, char **argv) {
  outshine::Render::PlanSpec declaration;
  declaration.Outputs = {outshine::Render::Resource::SceneDepth,
                         outshine::Render::Resource::FrameTex};
  declaration.Content = {outshine::Render::Stage::Subjects};
  declaration.Display =
      outshine::Render::Declared<outshine::Render::Transfer>(outshine::Render::Transfer::Linear);
  declaration.Exposure = outshine::Render::Declared<float>(1.0f);
  declaration.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
      outshine::Render::ScenePrecision::Float);
  std::shared_ptr<const outshine::Render::RenderPlan> plan;
  std::string why;
  CHECK(outshine::Render::RenderPlan::Compile(declaration, &plan, why),
        "the frame baseline's render declaration compiles");
  if (!plan) { return outshine::Test::Report(); }

  /* THE ARCHIVE IS READ BEFORE THIS RUN WRITES INTO IT, or a run would find itself and print a
   * comparison against its own numbers. */
  const std::vector<Record> earlier = ArchivedBefore();
  const outshine::Test::SourceIdentity sources = outshine::Test::SourcesUnderTest();

  outshine::Render::Renderer renderer;
  const auto initBegan = std::chrono::steady_clock::now();
  renderer.Init(kFrameWidthPx, kFrameHeightPx, plan);
  const double initMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - initBegan)
          .count();
  CHECK(renderer.DeviceUsable(), "the device came up, so a frame can be timed at all");
  if (!renderer.DeviceUsable()) { return outshine::Test::Report(); }

  std::printf("FRAME %dx%d budget %.4f ms, %d timed frames per arm after %d warm, %d repeats\n",
              kFrameWidthPx, kFrameHeightPx, kFrameBudgetMs, kTimedFrames, kWarmFrames, kRepeats);
  std::printf("SOURCE digest=%s files=%ld bytes=%ld population=src/+test/frame/ "
              "newest-source=%lld binary=%lld\n",
              sources.Digest.c_str(), sources.Files, sources.Bytes, sources.NewestModified,
              WhenBuilt(argv[0]));
  /* A DIGEST OVER NOTHING IS STILL SIXTY-FOUR HEX CHARACTERS. Run from anywhere but the repository
   * root the walk finds no file, and every record would then be filed under one key that identifies
   * no code at all -- which is the failure this whole field exists to end, wearing its own name. */
  CHECK(sources.Files > 0,
        "the source population the digest is taken over was found, so the digest identifies code");
  /* WHERE A PIPELINE COUNT IS PAID, PUBLISHED BESIDE WHAT IT COST. Every subject pipeline is built
   * inside `Init` and none of them inside a frame, so a row that adds permutations is charged here
   * and the frame lines below are where it is shown NOT to be charged. */
  std::printf("SETUP init=%.1f ms pipelines=%u\n", initMs, renderer.SubjectPipelineCount());

  const BoundRasters rasters;

  Standing standing;
  CHECK(ReadEveryArmsSubject(standing),
        "every declared arm's subject is in the tree, builds and frames itself");
  if (standing.Subjects.empty()) { return outshine::Test::Report(); }
  std::vector<Measured> measured(kArmCount);

  MeasureEveryArm(renderer, standing, rasters, measured);
  PublishEveryArm(standing, measured);

  /* THE TEXTURED ARM MUST DRAW A DIFFERENT PICTURE FROM THE FLAT ONE IT IS OTHERWISE IDENTICAL TO,
   * or its four sockets never reached a sampler and the arm timed the path it is named after in name
   * only. Both draw the same subject from the same standpoint under the same light, so the only
   * thing that can separate their radiance is the tap. */
  CHECK(measured[ArmNamed("texture")].SumRadiance != measured[ArmNamed("fill")].SumRadiance,
        "the textured arm's images reached the sampler, so it timed the tap path it is named for");

  /* THE KNOWN-SIGN CHANGE THIS INSTRUMENT PRICES EVERY TIME IT RUNS. One subject, one path, one more
   * shadow ray per surviving fragment: the cost cannot be negative and cannot be zero, so an
   * instrument that could not separate the two would be publishing distributions nothing can be
   * compared against. The bound is the two floors added, which is the widest a difference of two
   * measurements of unchanged binaries can be. */
  const Measured &lit = measured[ArmNamed("fill")];
  const Measured &twiceLit = measured[ArmNamed("fill-twice-lit")];
  const double priced = twiceLit.P50Ms() - lit.P50Ms();
  const double bound = lit.FloorMs() + twiceLit.FloorMs();
  std::printf("PRICED second-ray=%.3f ms floor-sum=%.3f ms resolved=%s\n", priced, bound,
              priced > bound ? "yes" : "no");
  CHECK(priced > bound,
        "the instrument resolves one more shadow ray per fragment above its own floor");

  CompareWithEarlierRuns(earlier, measured, sources.Digest);
  Archive(sources.Digest, measured);

  return outshine::Test::Report();
}
