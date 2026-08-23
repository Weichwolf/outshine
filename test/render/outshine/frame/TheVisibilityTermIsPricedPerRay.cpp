#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"

#include "Orbit.h"
#include "SourceDigest.h"
#include "WhatIsDrawn.h"

#include "Document.h"
#include "GltfStudio.h"
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

constexpr int kLightArms[] = {0, 1, 2, 8};

struct Subject {
  const char *Id;
  const char *Path;
};

constexpr Subject kSubjects[] = {
    {"water-bottle", "/test-render-khronos-glTF-WaterBottle/scene.glb"},
    {"normal-tangent-mirror", "/test-render-khronos-glTF-NormalTangentMirrorTest/NormalTangentMirrorTest.gltf"},
    {"normal-tangent", "/test-render-khronos-glTF-NormalTangentTest/NormalTangentTest.gltf"},
    {"scifi-helmet", "/test-render-khronos-glTF-SciFiHelmet/scene.gltf"},
    {"a-beautiful-game", "/test-render-khronos-glTF-ABeautifulGame/scene.gltf"},
};

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

outshine::Clients::Studio StudioOver(const outshine::Gltf::Subject &subject) {
  outshine::Clients::Studio studio;
  studio.Geometry = &subject;
  outshine::Render::SubjectMaterial surface;
  surface.Row.BaseColour[0] = surface.Row.BaseColour[1] = surface.Row.BaseColour[2] = 0.5f;
  surface.Row.BaseColour[3] = 1.0f;
  surface.Row.Roughness = 0.5f;
  surface.Row.Metalness = 0.0f;
  studio.Surfaces.push_back(surface);
  studio.EmittedRadiance.assign(subject.Parts().size(), {0.0f, 0.0f, 0.0f});
  studio.PartSurface.assign(subject.Parts().size(), 0u);
  return studio;
}

[[nodiscard]] bool TimeArm(outshine::Render::Renderer &renderer,
                           const outshine::Gltf::Subject &subject,
                           const outshine::Gltf::Placement &framed, double scale, int lightCount,
                           Distribution &out, std::string &error) {
  outshine::Clients::Studio studio = StudioOver(subject);
  studio.Lights = SunsFacingEveryWay(lightCount);
  outshine::Clients::StudioScratch scratch;

  studio.Eye = outshine::Test::OrbitAt(subject, framed, scale, 0, kTimedFrames);
  if (!outshine::Clients::Show(renderer, studio, scratch, error)) { return false; }
  for (int warm = 0; warm < kWarmFrames; ++warm) {
    if (!outshine::Clients::Aim(renderer, subject,
                                outshine::Test::OrbitAt(subject, framed, scale, warm, kTimedFrames),
                                error)) {
      return false;
    }
    renderer.RenderFrame();
  }
  renderer.WaitForGpu();

  std::vector<double> samples;
  samples.reserve(kTimedFrames);
  for (int step = 0; step < kTimedFrames; ++step) {
    if (!outshine::Clients::Aim(renderer, subject,
                                outshine::Test::OrbitAt(subject, framed, scale, step, kTimedFrames),
                                error)) {
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

double FillingScale(outshine::Render::Renderer &renderer, const outshine::Gltf::Subject &subject,
                    const outshine::Gltf::Placement &framed, long &coveredAt) {

  outshine::Clients::Studio studio = StudioOver(subject);
  outshine::Clients::StudioScratch scratch;
  std::string error;
  studio.Eye = outshine::Test::OrbitAt(subject, framed, 1.0, 0, kTimedFrames);
  if (!outshine::Clients::Show(renderer, studio, scratch, error)) {
    coveredAt = 0;
    return 1.0;
  }
  const double candidates[] = {1.0, 0.7, 0.5, 0.35, 0.25, 0.18, 0.12};
  double best = 1.0;
  coveredAt = 0;
  for (const double scale : candidates) {
    const outshine::Test::Drawn drawn =
        outshine::Test::WhatThePathDraws(renderer, subject, framed, scale, kTimedFrames, kProbes);
    if (drawn.MedianCoveredPx <= coveredAt) { continue; }
    coveredAt = drawn.MedianCoveredPx;
    best = scale;
  }
  return best;
}

}

int main(void) {
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
  CHECK([&] { auto made = outshine::Render::RenderPlan::Compile(declaration); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }(),
        "the frame instrument's render declaration compiles");
  if (!plan) { return outshine::Test::Report(); }

  outshine::Render::Renderer renderer;
  renderer.Init(kFrameWidthPx, kFrameHeightPx, plan);
  CHECK(renderer.DeviceUsable(), "the device came up, so a frame can be timed at all");
  if (!renderer.DeviceUsable()) { return outshine::Test::Report(); }

  const outshine::Test::SourceIdentity sources = outshine::Test::SourcesUnderTest();
  std::printf("FRAME %dx%d budget %.4f ms, %d timed frames per arm after %d warm\n", kFrameWidthPx,
              kFrameHeightPx, kFrameBudgetMs, kTimedFrames, kWarmFrames);
  std::printf("SOURCE digest=%s files=%ld bytes=%ld population=src/+test/frame/\n",
              sources.Digest.c_str(), sources.Files, sources.Bytes);

  for (const Subject &which : kSubjects) {
    outshine::Gltf::Document document;
    if (!document.ReadFile(PreparedRoot() + which.Path)) {
      std::printf("SUBJECT %s unread: %s\n", which.Id, document.Error().c_str());
      CHECK(false, "every declared subject of the frame instrument is in the tree");
      continue;
    }
    outshine::Gltf::Subject subject;
    if (!subject.Build(document)) {
      std::printf("SUBJECT %s unbuilt: %s\n", which.Id, subject.Error().c_str());
      CHECK(false, "every declared subject of the frame instrument builds");
      continue;
    }
    outshine::Gltf::Placement framed;
    if (!subject.Frame(framed)) {
      std::printf("SUBJECT %s unframed\n", which.Id);
      CHECK(false, "every declared subject of the frame instrument derives its own framing");
      continue;
    }

    std::printf("SUBJECT %s %zu triangles, %zu vertices\n", which.Id, subject.TriangleCount(),
                subject.VertexCount());

    long fillingCoveredPx = 0;
    const double filling = FillingScale(renderer, subject, framed, fillingCoveredPx);
    std::printf("PATH %s framing-scale=1.00 filling-scale=%.2f filling-covered=%ld px of %d "
                "(%.1f%% of the frame)\n",
                which.Id, filling, fillingCoveredPx, kFrameWidthPx * kFrameHeightPx,
                100.0 * (double)fillingCoveredPx / (double)(kFrameWidthPx * kFrameHeightPx));

    const double scales[] = {1.0, filling};
    for (const double scale : scales) {
      const outshine::Test::Drawn path =
          outshine::Test::WhatThePathDraws(renderer, subject, framed, scale, kTimedFrames, kProbes);
      double unlitP50Ms = 0.0;
      double unlitRadiance = 0.0;
      for (const int lights : kLightArms) {
        Distribution spread;
        std::string error;
        if (!TimeArm(renderer, subject, framed, scale, lights, spread, error)) {
          std::printf("SUBJECT %s at %d lights refused: %s\n", which.Id, lights, error.c_str());
          CHECK(false, "every timed arm rendered");
          continue;
        }
        const outshine::Test::Drawn drawn =
        outshine::Test::WhatThePathDraws(renderer, subject, framed, scale, kTimedFrames, kProbes);
        if (lights == kLightArms[0]) {
          unlitP50Ms = spread.P50Ms;
          unlitRadiance = drawn.SumRadiance;
        }

        const double perLightMs = lights > 0 ? (spread.P50Ms - unlitP50Ms) / (double)lights : 0.0;
        const double perRayNs = path.MedianCoveredPx > 0 && lights > 0
                                    ? perLightMs * 1.0e6 / (double)path.MedianCoveredPx
                                    : 0.0;

        const double wholeFrameMs = perRayNs * (double)(kFrameWidthPx * kFrameHeightPx) * 1.0e-6;
        std::printf("ARM %s scale=%.2f lights=%d covered=%ld p50=%.3f p95=%.3f p99=%.3f min=%.3f "
                    "max=%.3f perLight=%.4f ms perRay=%.2f ns wholeFrame=%.2f ms budget=%.1f%%\n",
                    which.Id, scale, lights, path.MedianCoveredPx, spread.P50Ms, spread.P95Ms,
                    spread.P99Ms, spread.MinMs, spread.MaxMs, perLightMs, perRayNs, wholeFrameMs,
                    100.0 * spread.P50Ms / kFrameBudgetMs);

        CHECK(path.MedianCoveredPx > 0,
              "the timed frames drew the subject rather than an empty target");

        if (lights > 0) {
          CHECK(drawn.SumRadiance > unlitRadiance,
                "a declared light reached the subject, so the arm timed the shading it claims to");
        }
      }
    }
  }

  return outshine::Test::Report();
}
