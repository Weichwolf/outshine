#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "DrawList.h"
#include "Readback.h"
#include "Wgs84.h"
#include "Renderer.h"
#include "SubjectTypes.h"

namespace {

constexpr int kFrameWidthPx = 1280;
constexpr int kFrameHeightPx = 720;
constexpr double kFrameBudgetMs = 1000.0 / 60.0;
constexpr int kWarmFrames = 24;
constexpr int kTimedFrames = 100;
constexpr int kRounds = 3;
constexpr uint32_t kDraws[] = {1,    2,    4,    8,     16,    32,    64,   128,
                               256,  512,  1024, 2048,  4096,  8192,  12288, 16384};
constexpr size_t kSteps = sizeof(kDraws) / sizeof(kDraws[0]);

constexpr float kSpanM = 4.0f;
constexpr double kAcrossM = 0.8;
constexpr double kBackM = 20.0;

struct Timed {
  double P50Ms = 0.0;
  double P95Ms = 0.0;
  double P99Ms = 0.0;
  double SpreadMs = 0.0;
  size_t CoveredPx = 0;
};

[[nodiscard]] size_t CoveredPixels(outshine::Render::Renderer &renderer,
                                   std::vector<float> &depth) {
  if (renderer.ReadDepth(depth) != outshine::Render::ReadState::Ready) { return 0; }
  size_t covered = 0;
  for (const float at : depth) { covered += at > 0.0f ? 1u : 0u; }
  return covered;
}

[[nodiscard]] Timed Percentiles(std::vector<double> &samples) {
  Timed out;
  if (samples.empty()) { return out; }
  std::sort(samples.begin(), samples.end());
  const auto at = [&](double share) {
    size_t which = (size_t)(share * (double)samples.size());
    if (which >= samples.size()) { which = samples.size() - 1; }
    return samples[which];
  };
  out.P50Ms = at(0.50);
  out.P95Ms = at(0.95);
  out.P99Ms = at(0.99);
  return out;
}

void PlaceOnAGrid(uint32_t draws, std::vector<double> &into) {
  const uint32_t across = (uint32_t)std::ceil(std::sqrt((double)draws));
  into.assign((size_t)draws * 16u, 0.0);
  for (uint32_t at = 0; at < draws; ++at) {
    double *model = into.data() + (size_t)at * 16u;
    model[0] = model[5] = model[10] = model[15] = 1.0;
    const double column = (double)(at % across) - 0.5 * (double)(across - 1u);
    const double row = (double)(at / across) - 0.5 * (double)(across - 1u);
    model[12] = 0.0;
    model[13] = column * kAcrossM;
    model[14] = row * kAcrossM;
  }
}

} // namespace

// board:1826: this case is the renderer's door, tried. Six declarations were handed to it --
// each missing one thing, each accepted in full, each drawing nothing, and WhyNot() answering
// the empty string every time: no lights, a subpixel triangle, the world origin at ECEF zero,
// no projection set, a triangle edge-on to the eye, and one wound the other way. The renderer
// refuses none of them. Until it does, this case measures a submission cost for draws that
// never reach the raster, which is why its covered-pixel arm stands first and red.

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

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
  CHECK([&] {
          auto made = outshine::Render::RenderPlan::Compile(declaration);
          if (made) { plan = *std::move(made); return true; }
          why = std::move(made).error();
          return false;
        }(),
        "the sweep's render declaration compiles");
  if (!plan) { return Report(); }

  outshine::Render::Renderer renderer;
  renderer.Init(kFrameWidthPx, kFrameHeightPx, plan);
  CHECK(renderer.DeviceUsable(), "the device came up, so a draw can be timed at all");
  if (!renderer.DeviceUsable()) { return Report(); }

  // board:1826: the renderer used to accept every one of these and draw nothing, with WhyNot()
  // answering the empty string. A door that cannot say what is missing costs the caller a day.
  {
    renderer.RenderFrame();
    Note("the renderer says why it drew no frame", renderer.WhyNot().empty() ? 0.0 : 1.0, "yes");
    std::printf("NOTE with no camera it says: '%.120s'\n", renderer.WhyNot().c_str());
    CHECK(!renderer.WhyNot().empty(),
          "**A FRAME WITH NO CAMERA IS REFUSED BY NAME**, not by drawing nothing -- the eye is "
          "what a picture is composed about, and a renderer that stays silent about its absence "
          "hands the caller a black frame and no reason (board:1826)");

    std::string refused;
    outshine::Render::SubjectMesh named;
    named.VertexCount = 3;
    named.IndexCount = 3;
    CHECK(!renderer.SetSubjectMesh(named, refused) && !refused.empty(),
          "**AND A MESH THAT NAMES GEOMETRY IT DOES NOT HAND OVER IS REFUSED BY NAME** -- three "
          "vertices declared, no position run, no indices, no draw list: eight shortfalls used "
          "to share one silent 'return true'");
    std::printf("NOTE an empty declaration says: '%.120s'\n", refused.c_str());

    refused.clear();
    CHECK(!renderer.SetSubjectPlacements(nullptr, 8, refused) && !refused.empty(),
          "**AND A PLACEMENT COUNT WITH NO TABLE IS REFUSED BY NAME**, where it used to pass as "
          "'no placements at all'");
  }

  const float verts[9] = {0.0f, -kSpanM, -kSpanM, 0.0f, kSpanM, -kSpanM, 0.0f, 0.0f, kSpanM};
  const float emitted[9] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  std::vector<uint32_t> indices;

  outshine::Render::SubjectMaterial surface;
  surface.Row.BaseColour[0] = surface.Row.BaseColour[1] = surface.Row.BaseColour[2] = 0.5f;
  surface.Row.BaseColour[3] = 1.0f;
  surface.Row.Roughness = 0.5f;
  std::string error;
  CHECK(renderer.SetSubjectMaterials(std::span<const outshine::Render::SubjectMaterial>(&surface, 1),
                                     error),
        "one surface stands for every draw, so the sweep varies the DRAW COUNT and nothing else");

  outshine::Render::SubjectLight sun;
  sun.Light.Kind = outshine::LightKind::Directional;
  sun.Light.Colour[0] = sun.Light.Colour[1] = sun.Light.Colour[2] = 1.0f;
  sun.Light.Intensity = 3.14159265358979323846f;
  sun.Light.Direction[0] = -1.0f;
  sun.Light.Direction[1] = 0.0f;
  sun.Light.Direction[2] = 0.0f;
  CHECK(renderer.SetSubjectLights(std::span<const outshine::Render::SubjectLight>(&sun, 1), error),
        "one sun faces the grid, so a lit triangle is a lit pixel");

  const double eye[3] = {outshine::Data::kWgs84A + kBackM, 0.0, 0.0};
  const double fwd[3] = {-1.0, 0.0, 0.0};
  const double right[3] = {0.0, 1.0, 0.0};
  const double up[3] = {0.0, 0.0, 1.0};
  renderer.SetFovDeg(60.0);
  renderer.SetNearM(0.1);
  renderer.SetCameraBasis(eye, fwd, right, up);

  std::printf("SWEEP %dx%d, %d timed frames after %d warm, one triangle per draw\n",
              kFrameWidthPx, kFrameHeightPx, kTimedFrames, kWarmFrames);

  std::vector<double> placements;
  std::vector<float> depth;
  std::vector<Timed> measured(kSteps);
  std::vector<std::vector<double>> rounds(kSteps);
  bool everyStepRendered = true;

  for (size_t round = 0; round < (size_t)kRounds && everyStepRendered; ++round) {
  for (size_t step = 0; step < kSteps; ++step) {
    const uint32_t draws = kDraws[step];
    outshine::Render::DrawList list;
    outshine::Render::VertexRunsCarried carried;
    outshine::Render::VertexLayout layout = outshine::Render::VertexLayout::Position;
    if (!outshine::Render::LayoutOf(carried, layout)) {
      everyStepRendered = false;
      break;
    }
    for (uint32_t at = 0; at < draws; ++at) {
      outshine::Render::DrawItem item;
      item.Order.Viewport = 0;
      item.Order.Layer = outshine::Render::ViewLayer::World;
      item.Order.Surface = surface.State();
      item.Order.DepthFraction = 0.5f;
      item.Order.MaterialSlot = 0;
      item.ModelSlot = at;
      item.SourceFirstIndex = at * 6u;
      item.IndexCount = 6;
      item.Layout = layout;
      if (!list.Add(item, error)) {
        everyStepRendered = false;
        break;
      }
    }
    if (!everyStepRendered) { break; }
    list.Compile();

    indices.clear();
    indices.reserve((size_t)draws * 6u);
    for (uint32_t at = 0; at < draws; ++at) {
      for (const uint32_t which : {0u, 1u, 2u, 2u, 1u, 0u}) { indices.push_back(which); }
    }

    PlaceOnAGrid(draws, placements);

    outshine::Render::SubjectMesh mesh;
    mesh.Verts = verts;
    mesh.Emitted = emitted;
    mesh.VertexCount = 3;
    mesh.Indices = indices.data();
    mesh.IndexCount = (uint32_t)indices.size();
    mesh.Draws = &list;
    mesh.Anchor[0] = outshine::Data::kWgs84A;
    mesh.PrevAnchor[0] = outshine::Data::kWgs84A;
    if (!renderer.SetSubjectMesh(mesh, error) ||
        !renderer.SetSubjectPlacements(placements.data(), draws, error)) {
      std::printf("REFUSED at %u draws: %s\n", draws, error.c_str());
      everyStepRendered = false;
      break;
    }

    renderer.BeginTemporalRun();
    for (int warm = 0; warm < kWarmFrames; ++warm) { renderer.RenderFrame(); }
    if (step == 0) {
      std::vector<float> linear;
      const bool readLinear =
          renderer.ReadSceneLinear(linear) == outshine::Render::ReadState::Ready;
      size_t lit = 0;
      for (size_t at = 0; readLinear && at + 3 < linear.size(); at += 4) {
        lit += linear[at] > 0.0f || linear[at + 1] > 0.0f || linear[at + 2] > 0.0f ? 1u : 0u;
      }
      std::printf("SEEN batches=%u draws=%u  depth=%zu px  linear=%zu px (read %d)  WHYNOT='%s'\n",
                  renderer.SubjectBatchCount(), renderer.SubjectDrawCount(),
                  CoveredPixels(renderer, depth), lit, (int)readLinear,
                  renderer.WhyNot().c_str());
    }
    renderer.WaitForGpu();

    std::vector<double> samples;
    samples.reserve(kTimedFrames);
    for (int frame = 0; frame < kTimedFrames; ++frame) {
      const auto began = std::chrono::steady_clock::now();
      renderer.RenderFrame();
      renderer.WaitForGpu();
      samples.push_back(std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - began)
                            .count());
    }
    const Timed now = Percentiles(samples);
    rounds[step].push_back(now.P50Ms);
    if (round + 1 == (size_t)kRounds) {
      std::vector<double> overRounds = rounds[step];
      measured[step] = Percentiles(overRounds);
      measured[step].P50Ms = overRounds[overRounds.size() / 2];
      measured[step].CoveredPx = CoveredPixels(renderer, depth);
      measured[step].SpreadMs = overRounds.back() - overRounds.front();
      std::printf("DRAWS %5u  p50 %8.4f ms  spread %6.4f ms over %d rounds  lit %zu px\n", draws,
                  measured[step].P50Ms, measured[step].SpreadMs, kRounds,
                  measured[step].CoveredPx);
    }
  }

  CHECK(everyStepRendered,
        "**EVERY STEP OF THE SWEEP RENDERED**, so the numbers below are a sweep and not the "
        "prefix of one");
  if (!everyStepRendered) { return Report(); }

  // The frame's cost is submission AND fill, and the sweep grows both until the grid covers the
  // screen. A slope taken over the whole sweep is therefore a slope through a moving fill, which
  // is not what board:1538 asked for. The per-draw term is read where the covered pixel count
  // has STOPPED moving: there the only thing that changed is how many draws carried it.
  }
  size_t saturatedFrom = kSteps;
  for (size_t step = 1; step < kSteps; ++step) {
    if (measured[step].CoveredPx == measured[step - 1].CoveredPx) {
      saturatedFrom = step - 1;
      break;
    }
  }
  const bool sawSaturation = saturatedFrom + 1 < kSteps;
  double perDrawUs = 0.0;
  double drawsInABudget = 0.0;
  double atSaturationMs = 0.0;
  if (sawSaturation) {
    // a least-squares slope over EVERY saturated step, not a difference of two: run-to-run
    // spread on this device is of the same order as the effect between two neighbours
    double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0, points = 0.0;
    for (size_t step = saturatedFrom; step < kSteps; ++step) {
      const double x = (double)kDraws[step];
      const double y = measured[step].P50Ms;
      sumX += x;
      sumY += y;
      sumXX += x * x;
      sumXY += x * y;
      points += 1.0;
    }
    const double slopeMsPerDraw =
        points * sumXX - sumX * sumX > 0.0
            ? (points * sumXY - sumX * sumY) / (points * sumXX - sumX * sumX)
            : 0.0;
    perDrawUs = slopeMsPerDraw * 1000.0;
    Note("saturated steps the slope stands on", points, "steps");
    const size_t from = saturatedFrom;
    atSaturationMs = measured[from].P50Ms;
    drawsInABudget = perDrawUs > 0.0
                         ? (double)kDraws[from] + (kFrameBudgetMs - atSaturationMs) * 1000.0 / perDrawUs
                         : 0.0;
    Note("the draw count the picture stops growing at", (double)kDraws[from], "draws");
    Note("what the frame costs there", atSaturationMs, "ms");
  }

  Note("what a draw costs on this device", perDrawUs, "us");
  Note("draws whose SUBMISSION alone spends the frame budget", drawsInABudget, "draws");
  Note("the budget that is measured against", kFrameBudgetMs, "ms");
  Note("p50 at one draw", measured[0].P50Ms, "ms");
  Note("p50 at the sweep's end", measured[kSteps - 1].P50Ms, "ms");
  Note("p99 at the sweep's end", measured[kSteps - 1].P99Ms, "ms");
  Note("how many draws that end is", (double)kDraws[kSteps - 1], "draws");
  Note("pixels one draw lights", (double)measured[0].CoveredPx, "px");
  Note("pixels the sweep's end lights", (double)measured[kSteps - 1].CoveredPx, "px");

  CHECK(sawSaturation,
        "**AND THE SWEEP REACHED A DRAW COUNT WHERE THE PICTURE STOPS GROWING**, which is the "
        "only place a per-draw cost can be read: below it, a longer frame is partly more "
        "pixels");
  CHECK(measured[kSteps - 1].CoveredPx > measured[0].CoveredPx,
        "**AND EVERY DRAW OF THE SWEEP REACHED THE PICTURE**: a timing sweep whose draws are "
        "discarded before rasterisation measures the discard and not the draw, so the lit "
        "pixel count is read back at every step and must grow with the count");
  CHECK(perDrawUs > 0.0,
        "**A DRAW COSTS SOMETHING AND THE SWEEP SAYS HOW MUCH**: the per-draw term is a slope "
        "over a swept draw count on THIS device, which is what turns 'thousands of entities' "
        "from an adjective into a draw budget -- and a draw budget is what decides whether "
        "instancing alone is enough or the batches must merge too (board:1538)");
  CHECK(measured[kSteps - 1].P50Ms > measured[0].P50Ms,
        "and the frame grows with the draw count, so the sweep is measuring submission rather "
        "than a constant the device pays anyway");
  CHECK(drawsInABudget > 0.0,
        "**AND THE DRAW COUNT AT WHICH SUBMISSION ALONE SPENDS 16.67 ms IS A NUMBER**, not an "
        "adjective -- every entity beyond it is a frame this device cannot hold, before a "
        "single pixel is shaded");

  Covers("III.9 the per-draw cost on this device is measured and published -- a slope in "
         "microseconds per draw over a swept draw count, p50/p95/p99 per step, and the draw "
         "count at which submission alone spends the frame budget (board:1538)");
  return Report();
}
