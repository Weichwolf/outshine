#include "Digest.h"
#include "math/Units.h"
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
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <vector>

#include "Fit.h"
#include "ReferenceLine.h"

#include "EngineHeld.h"
#include "GroundMesher.h"

namespace outshine {

namespace Says {
constexpr auto kColourReadbackIsBytes = "Colour requires the RGBA8 readback overload";
constexpr auto kUnknownReadbackBuffer = "unknown readback buffer";
constexpr auto kAudioAssemblyRequired =
    "assemble the current declaration before binding audio sources";
constexpr auto kAudioDeclarationRequired = "declare content before preparing audio";
constexpr auto kAudioPreparationRequired = "prepare audio before mixing";
constexpr auto kFrameAlreadyOpen = "a frame is already open on this engine";
constexpr auto kInvalidFrameExtent =
    "frame extent must be zero in both dimensions or positive in both";
}

namespace {
Result ValidateFrameExtent(Extent frame, const Seen &picture) {
  const bool defaultTarget = frame.WidthPx == 0 && frame.HeightPx == 0;
  if (!defaultTarget && (frame.WidthPx <= 0 || frame.HeightPx <= 0)) {
    return std::unexpected(std::string(Says::kInvalidFrameExtent));
  }
  if (frame.WidthPx > 0 && frame.HeightPx > 0 &&
      (frame.WidthPx != picture.Frame.WidthPx || frame.HeightPx != picture.Frame.HeightPx)) {
    return std::unexpected("this engine stands on a " + std::to_string(picture.Frame.WidthPx) +
                           "x" + std::to_string(picture.Frame.HeightPx) +
                           " canvas and was asked to draw " + std::to_string(frame.WidthPx) + "x" +
                           std::to_string(frame.HeightPx) +
                           " -- a canvas is declared before a scenario stands on it");
  }
  return {};
}

bool DrawScene(Seen &picture, std::string &error) {
  if (!picture.Standing->Draw(error)) { return false; }
  if (picture.Scope != FrameScope::Closed) { picture.Scope = FrameScope::DrawSucceeded; }
  return true;
}
}

Result Engine::prepareAudio(int sampleRateHz) {
  if (!S_->Session.Taken) {
    S_->Error = Says::kAudioDeclarationRequired;
    return std::unexpected(S_->Error);
  }
  const bool bound = std::ranges::any_of(
      S_->Session.Declared.Sounds, [](const Scenario::Sound &sound) { return !sound.On.empty(); });
  if (bound && S_->Simulation->DeclarationRevision != S_->Session.DeclarationRevision) {
    S_->Error = Says::kAudioAssemblyRequired;
    return std::unexpected(S_->Error);
  }
  Audio::Mixer candidate;
  auto setup =
      candidate.Stands(S_->Session.Declared.Buses, S_->Session.Declared.Sounds, sampleRateHz);
  if (!setup) {
    S_->Error = setup.error();
    return setup;
  }
  std::vector<std::optional<size_t>> bindings(S_->Session.Declared.Sounds.size());
  if (bound) {
    auto resolved = S_->Simulation->BindAudio(S_->Session.Declared.Sounds);
    if (!resolved) {
      S_->Error = resolved.error();
      return std::unexpected(S_->Error);
    }
    bindings = std::move(*resolved);
  }
  S_->Session.AudioBodies = std::move(bindings);
  S_->PublishAudioSnapshot();
  S_->Session.Sounding.emplace(std::move(candidate));
  S_->Error.clear();
  return {};
}

Result Engine::mix(std::span<float> stereo) {
  if (!S_->Session.Sounding) {
    S_->Error = Says::kAudioPreparationRequired;
    return std::unexpected(S_->Error);
  }
  const unsigned told = S_->Session.Told.load(std::memory_order_acquire);
  if (!S_->Session.Sounding->Fills(
          stereo, S_->Session.Sources[told], S_->Session.Ear[told], S_->Error)) {
    return std::unexpected(S_->Error);
  }
  S_->Error.clear();
  return {};
}

bool Engine::render(Extent frame) {
  if (const auto valid = ValidateFrameExtent(frame, S_->Picture); !valid) {
    S_->Error = valid.error();
    return false;
  }
  if (!S_->Stood()) { return false; }
  const auto began = std::chrono::steady_clock::now();
  if (!DrawScene(S_->Picture, S_->Error)) { return false; }
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
  if (!S_->Simulation->DynamicBodies.empty()) {
    S_->Published.Places("bodies standing on no route",
                         static_cast<double>(S_->Simulation->DynamicBodies.size()),
                         "bodies");
    S_->Published.Places(
        "the first of them, up", S_->Simulation->DynamicBodies.front().Motion.PositionM[1], "m");
    S_->Published.Places(
        "and how fast it falls", S_->Simulation->DynamicBodies.front().Motion.VelocityMs[1], "m/s");
  }
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
  if (!DrawScene(S_->Picture, S_->Error)) { return false; }
  return S_->Picture.Standing->ReadPixels(rgba, S_->Error);
}

bool Engine::readPixels(Buffer which, std::vector<float> &out) {
  switch (which) {
    case Buffer::Linear:
    case Buffer::Depth:
    case Buffer::ShadingNormal:
    case Buffer::SurfaceIdentity:
    case Buffer::Velocity: break;
    case Buffer::Colour: S_->Error = Says::kColourReadbackIsBytes; return false;
    default: S_->Error = Says::kUnknownReadbackBuffer; return false;
  }
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be read -- a scenario is declared before a frame carries pixels";
    return false;
  }
  if (!DrawScene(S_->Picture, S_->Error)) { return false; }
  return S_->Picture.Standing->ReadBuffer(which, out, S_->Error);
}

void Engine::logsTo(LogSink *sink) {
  outshine::Log::SetSink(sink);
}

Extent Engine::canvas() const {
  return S_->Picture.Targeted ? S_->Picture.Frame : Extent{};
}

bool Engine::camera(Camera &out) const {
  if (!S_->Picture.Standing) { return false; }
  Render::CameraOf(S_->Picture.Standing->Aimed(), out);
  return true;
}

bool Engine::presenting() const {
  return S_->Picture.Device.Presents();
}

bool Engine::beginFrame() {
  if (S_->Picture.Scope != FrameScope::Closed) {
    S_->Error = Says::kFrameAlreadyOpen;
    return false;
  }
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "a frame is begun over a scenario, and none stands";
    return false;
  }
  S_->Picture.Scope = FrameScope::Open;
  return true;
}

bool Engine::endFrame() {
  if (S_->Picture.Scope == FrameScope::Closed) {
    S_->Error = "a frame was ended that was never begun";
    return false;
  }
  const auto completed = std::exchange(S_->Picture.Scope, FrameScope::Closed);
  if (completed == FrameScope::DrawSucceeded || !S_->Picture.Standing ||
      !S_->Picture.Device.Presents()) {
    return true;
  }
  return DrawScene(S_->Picture, S_->Error);
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
  if (!DrawScene(S_->Picture, S_->Error)) { return false; }
  return S_->Picture.Standing->Screenshot(std::string(path), S_->Error);
}

}
