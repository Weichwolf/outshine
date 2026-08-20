/* WHAT ONE EXACT SHADOW RAY PER FRAGMENT COSTS AT 720p ON THIS DEVICE, measured rather than argued.
 *
 * THE INDEPENDENT VARIABLE IS THE NUMBER OF LIGHTS, and that is the whole design. A light is one
 * more ray per surviving fragment and nothing else changes -- same subject, same camera path, same
 * pipeline, same binary -- so the SLOPE of frame time against light count is the price of a ray and
 * it needs no second build to subtract. An arm with the ray switched off would be a dead path, which
 * this tree does not keep, and a second binary would be a second thing to attribute a difference to.
 *
 * THE ZERO-LIGHT ARM IS NOT A DISABLED FEATURE, it is a declaration: a subject nothing lights is
 * what every case outside `KHR_lights_punctual` states, and its cost is the frame with no shading
 * loop at all. It is the intercept the slope is taken from.
 *
 * THE CAMERA MOVES, because a frame cost is a distribution and a still frame has none (CLAUDE.md).
 * It orbits the subject at the framing distance the subject itself derives, so every frame draws a
 * different silhouette, a different covered-pixel count and a different set of traversals -- and the
 * spread between p50 and p99 is a property of the subject rather than of one lucky viewpoint.
 *
 * THE SUBJECTS ARE THE CORPUS'S OWN GEOMETRY AND NOT A STAND-IN. Traversal cost depends on how the
 * triangles are distributed, not only on how many there are, so a subdivided sphere at the same
 * count would be measuring a different structure. Five real assets span 4 510 to 1 500 224
 * triangles, which is two and a half orders and covers both the question this was built for -- does
 * the subject-sized case fit -- and the one behind it -- where does it stop fitting.
 *
 * IT IS NOT A CASE DIRECTORY AND HAS NO ORACLE. Nothing here is compared against Cycles; the picture
 * is the render suite's business and this is the clock's. What it DOES gate is the two things a
 * timing run can be wrong about without saying so: that every frame it timed actually drew the
 * subject, and that the frames it timed are the same picture every time. */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
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

/* THE FRAME THE FOURTH CONSTRAINT IS STATED AT (CLAUDE.md): 720p, and the budget that goes with it
 * is one sixtieth of a second. */
constexpr int kFrameWidthPx = 1280;
constexpr int kFrameHeightPx = 720;
constexpr double kFrameBudgetMs = 1000.0 / 60.0;

/* HOW MANY FRAMES ONE ARM IS TIMED OVER. [SET] 240: four seconds of a 60 Hz camera path, which puts
 * two frames past the 99th percentile so the p99 is a measured order statistic rather than the
 * maximum wearing a percentile's name. */
constexpr int kTimedFrames = 240;
/* THE SUBJECT IS SET UP ONCE PER ARM AND ONLY THE EYE MOVES AFTER THAT (`Clients::Aim`). Restating
 * the studio every frame would rebuild the draw list, repack the vertices and rebuild the
 * acceleration structure sixty times a second -- none of it inside the timed span, all of it in the
 * run's wall clock, and at 1.5 M triangles it was most of a nine-minute run. */
/* Frames drawn and thrown away before the clock starts, so the first-use cost of a pipeline, a
 * buffer and a texture is not counted as a frame's. */
constexpr int kWarmFrames = 20;
/* HOW MANY POINTS OF THE PATH THE PICTURE IS SAMPLED AT. [SET] 8: a coverage read is a fence and a
 * copy, so it is taken often enough for the median to be an order statistic and rarely enough that
 * it is not most of the run. */
constexpr int kProbes = 8;

/* THE LIGHT COUNTS THE SLOPE IS TAKEN OVER. Zero is the intercept; the rest double, so a cost that
 * is linear in the ray count and one that is not are told apart by four points rather than two. */
constexpr int kLightArms[] = {0, 1, 2, 8};

struct Subject {
  const char *Id;
  const char *Path;
};

/* THE CORPUS'S OWN ASSETS, in the order of their triangle counts, NAMED BY THEIR LEAF UNDER THE
 * PREPARED ROOT (board:1364). Every one is a render case's subject and none of them is in the tree:
 * a case directory carries its manifest and nothing else, so the path is the prepared root plus the
 * case's own flattened directory -- and these two tests were still naming the tree when the products
 * moved out of it. The table stays `constexpr` and the root is prepended where it is read, because
 * the root is a runtime answer about this machine's temp directory. */
constexpr Subject kSubjects[] = {
    {"water-bottle", "/test-render-khronos-glTF-WaterBottle/scene.glb"},
    {"normal-tangent-mirror", "/test-render-khronos-glTF-NormalTangentMirrorTest/NormalTangentMirrorTest.gltf"},
    {"normal-tangent", "/test-render-khronos-glTF-NormalTangentTest/NormalTangentTest.gltf"},
    {"scifi-helmet", "/test-render-khronos-glTF-SciFiHelmet/scene.gltf"},
    {"a-beautiful-game", "/test-render-khronos-glTF-ABeautifulGame/scene.gltf"},
};

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

/* THE LIGHTS ONE ARM DECLARES: `count` suns pointing in different directions, all delta, all
 * reaching the subject. They are spread rather than stacked because two lights from one direction
 * would trace two rays down one path and the second would be free in the cache -- which would price
 * a ray at less than a ray costs. */
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

/* THE SUBJECT AS THE STUDIO TAKES IT: every part opaque, mid-grey, half rough, no image anywhere.
 * The appearance is deliberately not the file's -- a texture tap is a cost this measurement is not
 * about, and the traversal is the same whichever arm samples what. */
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

/* ONE ARM, TIMED. Returns false only where the render itself refused -- a timing number taken from
 * a frame that drew nothing is the one way this instrument could lie. */
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

/* THE DISTANCE THE SUBJECT FILLS THE FRAME AT, found rather than assumed. The subject's own framing
 * puts it at the size it was built to be looked at, which for this corpus is a few per cent of a
 * 720p frame -- and the question the ruling asks is about ONE RAY PER SCREEN PIXEL, which is a
 * frame-filling subject. So the orbit is pulled in until the median coverage stops rising or the
 * placement is refused, and which scale won is published beside every number taken at it. */
double FillingScale(outshine::Render::Renderer &renderer, const outshine::Gltf::Subject &subject,
                    const outshine::Gltf::Placement &framed, long &coveredAt) {
  /* THE SUBJECT HAS TO BE STANDING BEFORE ITS COVERAGE CAN BE ASKED. `Aim` moves the eye and sets no
   * mesh, so probing before the first `Show` measured whatever the renderer last held -- which for
   * the first subject of a run is nothing at all, and it reported a coverage of zero. */
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

} // namespace

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
  CHECK(outshine::Render::RenderPlan::Compile(declaration, &plan, why),
        "the frame instrument's render declaration compiles");
  if (!plan) { return outshine::Test::Report(); }

  outshine::Render::Renderer renderer;
  renderer.Init(kFrameWidthPx, kFrameHeightPx, plan);
  CHECK(renderer.DeviceUsable(), "the device came up, so a frame can be timed at all");
  if (!renderer.DeviceUsable()) { return outshine::Test::Report(); }

  /* WHICH CODE THESE DURATIONS BELONG TO (board:1187). A frame number with no source identity beside
   * it cannot be compared against another run's, which is what left this suite unable to price a
   * change for as long as it has existed. */
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
        /* THE PRICE OF ONE RAY, and it is a per-fragment number rather than a per-frame one: the
         * frame's excess over the unlit arm, divided by the rays that excess bought. */
        const double perLightMs = lights > 0 ? (spread.P50Ms - unlitP50Ms) / (double)lights : 0.0;
        const double perRayNs = path.MedianCoveredPx > 0 && lights > 0
                                    ? perLightMs * 1.0e6 / (double)path.MedianCoveredPx
                                    : 0.0;
        /* WHAT ONE RAY PER SCREEN PIXEL WOULD COST AT THIS PRICE, which is the number the ruling
         * asks for: 921 600 rays a frame against this subject's structure. */
        const double wholeFrameMs = perRayNs * (double)(kFrameWidthPx * kFrameHeightPx) * 1.0e-6;
        std::printf("ARM %s scale=%.2f lights=%d covered=%ld p50=%.3f p95=%.3f p99=%.3f min=%.3f "
                    "max=%.3f perLight=%.4f ms perRay=%.2f ns wholeFrame=%.2f ms budget=%.1f%%\n",
                    which.Id, scale, lights, path.MedianCoveredPx, spread.P50Ms, spread.P95Ms,
                    spread.P99Ms, spread.MinMs, spread.MaxMs, perLightMs, perRayNs, wholeFrameMs,
                    100.0 * spread.P50Ms / kFrameBudgetMs);

        CHECK(path.MedianCoveredPx > 0,
              "the timed frames drew the subject rather than an empty target");
        /* A LIT ARM MUST DIFFER FROM THE UNLIT ONE IN THE PICTURE, or the ray was priced over a
         * shading loop that never ran. */
        if (lights > 0) {
          CHECK(drawn.SumRadiance > unlitRadiance,
                "a declared light reached the subject, so the arm timed the shading it claims to");
        }
      }
    }
  }

  return outshine::Test::Report();
}
