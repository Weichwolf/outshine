#include "Digest.h"
#include "Units.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "Log.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <expected>
#include <memory>
#include <cmath>
#include "Heap.h"
#include "TangentFrame.h"
#include <array>
#include <optional>
#include <span>
#include <numbers>
#include <string>
#include <ratio>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <vector>

#include "Fit.h"
#include "ReferenceLine.h"

#include "EngineHeld.h"
#include "GroundYield.h"

namespace outshine {

Result Engine::mix(std::span<float> stereo, int rate) {
  if (!S_->Session.Mixing) {
    if (!S_->Session.Sounding.Stands(
            S_->Session.Declared.Buses, S_->Session.Declared.Sounds, rate, S_->Error)) {
      return std::unexpected(S_->Error);
    }
    S_->Session.Mixing = true;
  }
  const unsigned told = S_->Session.Told.load(std::memory_order_acquire);
  return S_->Session.Sounding.Fills(
             stereo, S_->Session.Sources[told], S_->Session.Ear[told], S_->Error)
             ? Result{}
             : std::unexpected(S_->Error);
}

bool Engine::render(Extent frame) {
  if (!S_->Stood()) { return false; }
  if (frame.WidthPx > 0 && frame.HeightPx > 0 &&
      (frame.WidthPx != S_->Picture.Frame.WidthPx ||
       frame.HeightPx != S_->Picture.Frame.HeightPx)) {
    S_->Error = "this engine stands on a " + std::to_string(S_->Picture.Frame.WidthPx) + "x" +
                std::to_string(S_->Picture.Frame.HeightPx) + " canvas and was asked to draw " +
                std::to_string(frame.WidthPx) + "x" + std::to_string(frame.HeightPx) +
                " -- a canvas is declared before a scenario stands on it";
    return false;
  }
  const auto began = std::chrono::steady_clock::now();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  S_->Cost.Render.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  S_->Published.Places(
      "subject draws", static_cast<double>(S_->Picture.Device.SubjectDrawCount()), "draws");
  S_->Published.Places(
      "the subject's own animation runs for", S_->Picture.Standing->DurationS(), "s");
  S_->Published.Places("the frames its rate makes of that",
                       static_cast<double>(S_->Picture.Standing->Frames()),
                       "frames");
  S_->Published.Places("and the instant it is posed at", S_->Picture.Standing->AtS(), "s");
  S_->Published.Places(
      "the pose's own local transforms, digested", S_->Picture.Standing->LocalsDigest(), "");
  S_->Published.Places(
      "the vertices it assembled from them, digested", S_->Picture.Standing->AssembledDigest(), "");
  S_->Published.Places(
      "the geometry the renderer was last offered, digested", Render::HandedGeometryDigest(), "");
  S_->Published.Places("uploads the subject residency has made in all",
                       static_cast<double>(Render::SubjectResidency::UploadsEver()),
                       "uploads");
  S_->Published.Places("staged crossings the residency flushed",
                       static_cast<double>(Render::SubjectResidency::CrossingsFlushed()),
                       "crossings");
  S_->Published.Places("subject clusters",
                       static_cast<double>(S_->Picture.Standing->Shown().Clusters.size()),
                       "clusters");
  S_->Published.Places("cull: jobs it swept",
                       static_cast<double>(Render::SubjectCullStage::JobsSweptTaken()),
                       "jobs");
  if (S_->Session.Declared.Render.Audits) {
    Render::PyramidDepths depths;
    if (S_->Picture.Standing->Pyramid(depths) == Render::ReadState::Ready) {
      S_->Published.Places(
          "cull: the pyramid's nearest depth", static_cast<double>(depths.Nearest), "0..1");
      S_->Published.Places("cull: its farthest", static_cast<double>(depths.Farthest), "0..1");
      S_->Published.Places("cull: and its mean", static_cast<double>(depths.Mean), "0..1");
    }
  }
  {
    const Render::Viewpoint &eye = S_->Picture.Standing->Aimed();
    const double aspect = S_->Picture.Frame.HeightPx > 0
                              ? static_cast<double>(S_->Picture.Frame.WidthPx) /
                                    static_cast<double>(S_->Picture.Frame.HeightPx)
                              : 1.0;
    const double half = 0.5 * eye.YfovRad;
    const double up = std::tan(half);
    const double across = up * aspect;
    size_t kept = 0;
    for (const DagCluster &one : S_->Picture.Standing->Shown().Clusters) {
      const Vec3 to = {{static_cast<double>(one.SelfCenter[0]) - eye.EyeM[0],
                        static_cast<double>(one.SelfCenter[1]) - eye.EyeM[1],
                        static_cast<double>(one.SelfCenter[2]) - eye.EyeM[2]}};
      const double ahead = to[0] * eye.Forward[0] + to[1] * eye.Forward[1] + to[2] * eye.Forward[2];
      const double right = to[0] * eye.Right[0] + to[1] * eye.Right[1] + to[2] * eye.Right[2];
      const double over = to[0] * eye.Up[0] + to[1] * eye.Up[1] + to[2] * eye.Up[2];
      const auto radius = static_cast<double>(one.SelfRadius);
      if (ahead + radius < eye.ZNearM) { continue; }
      if (eye.ZFarM > 0.0 && ahead - radius > eye.ZFarM) { continue; }
      if (std::fabs(right) - radius > across * (ahead > 0.0 ? ahead : 0.0) + radius) { continue; }
      if (std::fabs(over) - radius > up * (ahead > 0.0 ? ahead : 0.0) + radius) { continue; }
      ++kept;
    }
    S_->Published.Places(
        "ring: clusters a frustum would keep", static_cast<double>(kept), "clusters");
  }
  S_->Published.Places(
      "subject draw calls", static_cast<double>(S_->Picture.Device.SubjectBatchCount()), "calls");
  S_->Published.Places(
      "plan passes", static_cast<double>(S_->Picture.Standing->PlanPasses()), "passes");
  for (uint32_t at = 0; at < static_cast<uint32_t>(Render::kVertexLayouts.size()); ++at) {
    const uint32_t many =
        S_->Picture.Device.SubjectBatchesTaking(static_cast<Render::VertexLayout>(at));
    if (many == 0) { continue; }
    S_->Published.Places(
        "draws taking vertex layout " + std::to_string(at), static_cast<double>(many), "draws");
  }
  S_->Drew();
  return true;
}

Result Engine::inspect() {
  if (!S_->Stood()) { return std::unexpected(S_->Error); }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be inspected -- a scenario is declared before a frame carries "
                "anything a readback could tell";
    return std::unexpected(S_->Error);
  }
  S_->Inspected();
  return {};
}

bool Engine::readPixels(std::vector<uint8_t> &rgba) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be read -- a scenario is declared before a frame carries pixels";
    return false;
  }
  S_->Picture.Device.WantsPixels();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  return S_->Picture.Standing->ReadPixels(rgba, S_->Error);
}

bool Engine::readPixels(Buffer which, std::vector<float> &out) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be read -- a scenario is declared before a frame carries pixels";
    return false;
  }
  S_->Picture.Device.WantsPixels();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  return S_->Picture.Standing->ReadBuffer(which, out, S_->Error);
}

void Engine::logsTo(LogSink *sink) {
  outshine::Log::SetSink(sink);
}

Extent Engine::canvas() const {
  return S_->Picture.Frame;
}

bool Engine::camera(Scenario::Camera &out) const {
  if (!S_->Picture.Standing) { return false; }
  Render::CameraOf(S_->Picture.Standing->Aimed(), out);
  return true;
}

bool Engine::presenting() const {
  return S_->Picture.Device.Presents();
}

bool Engine::beginFrame() {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "a frame is begun over a scenario, and none stands";
    return false;
  }
  S_->Picture.FrameOpen = true;
  return true;
}

bool Engine::endFrame() {
  if (!S_->Picture.FrameOpen) {
    S_->Error = "a frame was ended that was never begun";
    return false;
  }
  S_->Picture.FrameOpen = false;
  if (!S_->Picture.Standing) { return true; }
  return S_->Picture.Standing->Present(S_->Error);
}

bool Engine::flushAndWait() {
  if (!S_->Picture.Standing) { return true; }
  return S_->Picture.Standing->Settle(S_->Error);
}

bool Engine::saveScreenshot(std::string_view path) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be captured -- a scenario is declared before a frame is kept";
    return false;
  }
  S_->Picture.Device.WantsPixels();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  return S_->Picture.Standing->Screenshot(std::string(path), S_->Error);
}

} // namespace outshine
