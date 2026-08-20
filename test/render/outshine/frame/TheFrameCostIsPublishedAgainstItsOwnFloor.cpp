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

#include "PreparedRoot.h"

#include "Orbit.h"
#include "SourceDigest.h"
#include "WhatIsDrawn.h"

#include "Document.h"
#include "GltfStudio.h"
#include "Image.h"
#include "RenderPlan.h"
#include "Renderer.h"
#include "Subject.h"

using outshine::Test::PreparedRoot;

namespace {

constexpr int kFrameWidthPx = 1280;
constexpr int kFrameHeightPx = 720;
constexpr double kFrameBudgetMs = 1000.0 / 60.0;

constexpr int kTimedFrames = 240;

constexpr int kWarmFrames = 20;

constexpr int kProbes = 8;

constexpr int kRepeats = 5;

enum class Appearance { Flat, Textured, Transmissive };

struct Arm {
  const char *Id;
  const char *Subject;
  const char *Path;

  double Scale;
  int Lights;
  Appearance Skin;
};

constexpr Arm kArms[] = {
    {"geometry", "a-beautiful-game", "/test-render-khronos-glTF-ABeautifulGame/scene.gltf", 1.00, 0,
     Appearance::Flat},
    {"fill", "scifi-helmet", "/test-render-khronos-glTF-SciFiHelmet/scene.gltf", 0.25, 1,
     Appearance::Flat},
    {"fill-twice-lit", "scifi-helmet", "/test-render-khronos-glTF-SciFiHelmet/scene.gltf", 0.25, 2,
     Appearance::Flat},
    {"texture", "scifi-helmet", "/test-render-khronos-glTF-SciFiHelmet/scene.gltf", 0.25, 1,
     Appearance::Textured},

    {"fill-unlit", "scifi-helmet", "/test-render-khronos-glTF-SciFiHelmet/scene.gltf", 0.25, 0,
     Appearance::Flat},

    {"transmissive", "scifi-helmet", "/test-render-khronos-glTF-SciFiHelmet/scene.gltf", 0.25, 1,
     Appearance::Transmissive},
};

constexpr size_t kArmCount = sizeof(kArms) / sizeof(kArms[0]);

[[nodiscard]] constexpr size_t ArmNamed(std::string_view id) {
  for (size_t at = 0; at < kArmCount; ++at) {
    if (id == kArms[at].Id) { return at; }
  }
  return kArmCount;
}
static_assert(ArmNamed("fill") < kArmCount && ArmNamed("fill-twice-lit") < kArmCount,
              "the two arms the known-sign gate is taken over are in the table");

constexpr uint32_t kRasterSide = 512;

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

outshine::Clients::Studio StudioOver(const outshine::Gltf::Subject &subject, Appearance skin,
                                     const BoundRasters &rasters) {
  outshine::Clients::Studio studio;
  studio.Geometry = &subject;
  outshine::Render::SubjectMaterial surface;
  surface.Row.BaseColour[0] = surface.Row.BaseColour[1] = surface.Row.BaseColour[2] = 0.5f;
  surface.Row.BaseColour[3] = 1.0f;
  surface.Row.Roughness = 0.5f;
  surface.Row.Metalness = 0.0f;
  if (skin == Appearance::Transmissive) { surface.Row.Transmission = 1.0f; }
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

struct Measured {
  std::vector<Distribution> Repeats;
  long CoveredPx = 0;
  long LeastCoveredPx = 0;
  long MostCoveredPx = 0;
  double SumRadiance = 0.0;
  bool EveryRepeatDrewTheSamePicture = true;
  bool Rendered = true;
  std::string Refusal;

  [[nodiscard]] std::vector<double> Across(double Distribution::*which) const {
    std::vector<double> out;
    for (const Distribution &repeat : Repeats) { out.push_back(repeat.*which); }
    std::sort(out.begin(), out.end());
    return out;
  }

  [[nodiscard]] double MedianOf(double Distribution::*which) const {
    const std::vector<double> sorted = Across(which);
    return sorted.empty() ? 0.0 : sorted[sorted.size() / 2];
  }
  [[nodiscard]] double P50Ms() const { return MedianOf(&Distribution::P50Ms); }
  [[nodiscard]] double P95Ms() const { return MedianOf(&Distribution::P95Ms); }
  [[nodiscard]] double P99Ms() const { return MedianOf(&Distribution::P99Ms); }

  [[nodiscard]] double FloorMs() const {
    const std::vector<double> sorted = Across(&Distribution::P50Ms);
    return sorted.empty() ? 0.0 : sorted.back() - sorted.front();
  }
};

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

  renderer.BeginTemporalRun();

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

std::filesystem::path ArchiveDirectory(void) {
  const char *temp = std::getenv("TMPDIR");
  return std::filesystem::path(temp && *temp ? temp : "/tmp") / "outshine-frame";
}

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

long long WhenBuilt(const char *binary) {
  std::error_code failed;
  const auto written = std::filesystem::last_write_time(binary, failed);
  if (failed) { return 0; }
  return (long long)std::chrono::duration_cast<std::chrono::seconds>(written.time_since_epoch())
      .count();
}

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
    if (!out.Documents[at].ReadFile(PreparedRoot() + kArms[at].Path)) {
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

std::shared_ptr<const outshine::Render::RenderPlan> PlanFor(Appearance skin) {
  outshine::Render::PlanSpec declaration;
  declaration.Outputs = {outshine::Render::Resource::SceneDepth,
                         outshine::Render::Resource::FrameTex};
  declaration.Content = {outshine::Render::Stage::Subjects,
                         outshine::Render::Stage::TemporalResolve};
  if (skin == Appearance::Transmissive) {
    declaration.Content.push_back(outshine::Render::Stage::SubjectsTransmissive);
  }
  declaration.Display =
      outshine::Render::Declared<outshine::Render::Transfer>(outshine::Render::Transfer::Linear);
  declaration.Exposure = outshine::Render::Declared<float>(1.0f);
  declaration.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
      outshine::Render::ScenePrecision::Float);
  std::shared_ptr<const outshine::Render::RenderPlan> plan;
  std::string why;
  if (!outshine::Render::RenderPlan::Compile(declaration, &plan, why)) { return nullptr; }
  return plan;
}

void MeasureEveryArm(outshine::Render::Renderer &renderer, const Standing &standing,
                     const BoundRasters &rasters, std::vector<Measured> &measured) {
  Appearance standing_plan = Appearance::Flat;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    for (size_t at = 0; at < kArmCount; ++at) {
      const Arm &arm = kArms[at];
      const Appearance wanted =
          arm.Skin == Appearance::Transmissive ? Appearance::Transmissive : Appearance::Flat;
      if (wanted != standing_plan) {
        renderer.Init(kFrameWidthPx, kFrameHeightPx, PlanFor(wanted));
        standing_plan = wanted;
      }
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

      if (drawn.NonFinitePx > 0) {
        std::printf("NONFINITE %s repeat %d: %ld covered pixels are not finite, first at index %ld "
                    "carrying (%g, %g, %g, %g) at depth %g\n",
                    arm.Id, repeat, drawn.NonFinitePx, drawn.FirstNonFiniteAt,
                    (double)drawn.FirstNonFinite[0], (double)drawn.FirstNonFinite[1],
                    (double)drawn.FirstNonFinite[2], (double)drawn.FirstNonFinite[3],
                    (double)drawn.FirstNonFiniteDepth);
      }
      CHECK(drawn.NonFinitePx == 0,
            "every covered pixel of every probe carries a finite radiance, so the picture is a "
            "number and the sums over it are about the picture");
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

        std::printf("DIFFERS %s repeat %d: covered %ld against %ld (%+ld px), radiance %.9g against "
                    "%.9g (%+.3g, %+.4g %%)\n",
                    arm.Id, repeat, drawn.MedianCoveredPx, measured[at].CoveredPx,
                    drawn.MedianCoveredPx - measured[at].CoveredPx, drawn.SumRadiance,
                    measured[at].SumRadiance, drawn.SumRadiance - measured[at].SumRadiance,
                    measured[at].SumRadiance != 0.0
                        ? 100.0 * (drawn.SumRadiance - measured[at].SumRadiance) /
                              measured[at].SumRadiance
                        : 0.0);
      }
      std::printf("REPEAT %s %d setup=%.1f ms p50=%.3f p95=%.3f p99=%.3f min=%.3f max=%.3f\n",
                  arm.Id, repeat, setupMs, spread.P50Ms, spread.P95Ms, spread.P99Ms, spread.MinMs,
                  spread.MaxMs);
    }
  }
}

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
                arm.Skin == Appearance::Textured ? "textured"
                    : arm.Skin == Appearance::Transmissive ? "transmissive"
                                                           : "flat",
                result.CoveredPx,
                kFrameWidthPx * kFrameHeightPx, result.LeastCoveredPx, result.MostCoveredPx,
                kTimedFrames, kRepeats, result.P50Ms(), result.P95Ms(), result.P99Ms(),
                result.FloorMs(), 100.0 * result.FloorMs() / result.P50Ms(),
                100.0 * result.P50Ms() / kFrameBudgetMs);
    CHECK(result.CoveredPx > 0, "the timed frames drew the subject rather than an empty target");
    CHECK(result.EveryRepeatDrewTheSamePicture,
          "every repeat of an arm drew the same picture, so the repeats are of one thing");
  }
}

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

      std::printf("COMPARE %s arm=%s archived=%.3f thisrun=%.3f delta=%+.3f ms (%+.1f%%) "
                  "floor-sum=%.3f ms %s archived-digest=%s archived-at=%lld\n",
                  before.Digest == digest ? "SAME-SOURCE" : "CHANGED-SOURCE", kArms[at].Id,
                  before.P50Ms, now, delta, 100.0 * delta / before.P50Ms, width,
                  std::fabs(delta) > width ? "RESOLVED" : "within-floor", before.Digest.c_str(),
                  before.Ran);
    }
  }
}

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

}

int main(int, char **argv) {
  outshine::Render::PlanSpec declaration;
  declaration.Outputs = {outshine::Render::Resource::SceneDepth,
                         outshine::Render::Resource::FrameTex};

  declaration.Content = {outshine::Render::Stage::Subjects,
                         outshine::Render::Stage::TemporalResolve};
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

  CHECK(sources.Files > 0,
        "the source population the digest is taken over was found, so the digest identifies code");

  std::printf("SETUP init=%.1f ms pipelines=%u\n", initMs, renderer.SubjectPipelineCount());

  const BoundRasters rasters;

  Standing standing;
  CHECK(ReadEveryArmsSubject(standing),
        "every declared arm's subject is in the tree, builds and frames itself");
  if (standing.Subjects.empty()) { return outshine::Test::Report(); }
  std::vector<Measured> measured(kArmCount);

  MeasureEveryArm(renderer, standing, rasters, measured);
  PublishEveryArm(standing, measured);

  CHECK(measured[ArmNamed("texture")].SumRadiance != measured[ArmNamed("fill")].SumRadiance,
        "the textured arm's images reached the sampler, so it timed the tap path it is named for");

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
