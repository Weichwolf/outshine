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
#include <string_view>
#include <ratio>
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
constexpr auto NoRenderTarget = "a render target is required before creating the scene";
}

constexpr float kNearestOccluderM = 0.01f;

bool Engine::State::Stood() {
  if (!Picture.Standing) {
    if (!Picture.Targeted) {
      Error = Says::NoRenderTarget;
      return false;
    }
    Core::Declaration wanted = Picture.Shown;
    wanted.SurfaceWidthPx = Picture.Frame.WidthPx;
    wanted.SurfaceHeightPx = Picture.Frame.HeightPx;
    if (Picture.PendingGeometry) { wanted.InitialGeometry = &*Picture.PendingGeometry; }
    if (!Core::RuntimeScene::Open(
            Picture.Device, std::move(wanted), &Picture.Face, Picture.Standing, Error)) {
      return false;
    }
    Picture.PendingGeometry.reset();
    if (Picture.PendingAudioOcclusion) {
      World.AudioOcclusion = std::move(*Picture.PendingAudioOcclusion);
      Picture.PendingAudioOcclusion.reset();
    }
  }
  if (!Picture.PendingGeometry) { return true; }
  if (!Core::RuntimeScene::ReplacesGeometry(Picture.Device,
                                            *Picture.Standing,
                                            Picture.PendingGeometry->clone(),
                                            &Picture.Face,
                                            Picture.Standing,
                                            Error)) {
    return false;
  }
  World.BindSceneResources(Picture.Device);
  Picture.PendingGeometry.reset();
  if (Picture.PendingAudioOcclusion) {
    World.AudioOcclusion = std::move(*Picture.PendingAudioOcclusion);
    Picture.PendingAudioOcclusion.reset();
  }
  return true;
}

void Engine::State::TellResourcePayloads() {
  if (!Picture.Standing) { return; }
  Published.Places("streamed piece CPU payload capacity",
                   static_cast<double>(Picture.Device.PieceSourceBytes()),
                   "bytes");
  Published.Places("streamed piece slot capacity",
                   static_cast<double>(Picture.Device.PieceSlotBytes()),
                   "bytes");
  Published.Places("height page slot capacity",
                   static_cast<double>(Picture.Device.HeightPageSlotBytes()),
                   "bytes");
  Published.Places("height page CPU payload capacity",
                   static_cast<double>(Picture.Device.HeightPageSourceBytes()),
                   "bytes");
}

void Engine::State::Tells() {
  static const Heap::Tag kTellingTag("frame-tells");
  const Heap::Tagged telling(kTellingTag);
  if (Heap::ProcessInstrumentationEnabled()) {
    TellResourcePayloads();
    Published.Places(
        "process C++ heap live bytes", static_cast<double>(Heap::LiveBytes()), "bytes");
    for (size_t at = 0; at < Heap::TagCount(); ++at) {
      const char *const tag = Heap::TagAt(at);
      if (tag == nullptr || Heap::TakenAt(at) == 0) { continue; }
      Published.Places(std::string("process C++ bytes allocated under ") + tag,
                       static_cast<double>(Heap::TakenAt(at)),
                       "bytes");
    }
  }
  if (Cost.Advance.Taken() > 0) {
    Published.Places("the step's own time, last", Cost.Advance.LastMs(), "ms");
    Published.Places("the step's own time, least", Cost.Advance.LeastMs(), "ms");
    Published.Places("the step's own time, most", Cost.Advance.MostMs(), "ms");
    Published.Places("steps taken", static_cast<double>(Cost.Advance.Taken()), "steps");
    Published.Places("world update time, most", Cost.Update.MostMs(), "ms");
    Published.Places("measurement publication time, most", Cost.Telling.MostMs(), "ms");
    Published.Places("scene advance time, most", Cost.SceneAdvance.MostMs(), "ms");
    Published.Places("streaming and bake time, most", Cost.Streaming.MostMs(), "ms");
    Published.Places("piece handoff time, most", Cost.PieceHandoff.MostMs(), "ms");
    Published.Places("tile restand time, most", Cost.Restand.MostMs(), "ms");
    Published.Places("structure bake time, most", Cost.Bakes.MostMs(), "ms");
    Published.Places("structure worker collection time, most", Cost.BakeResume.MostMs(), "ms");
    Published.Places("structure landing selection time, most", Cost.BakeLanding.MostMs(), "ms");
    Published.Places("structure transfer time, most", Cost.BakeTransfer.MostMs(), "ms");
    Published.Places("structure live transfer time, most", Cost.BakeLiveTransfer.MostMs(), "ms");
    Published.Places(
        "structure candidate transfer time, most", Cost.BakeCandidateTransfer.MostMs(), "ms");
    Published.Places("structure landing commit time, most", Cost.BakeCommit.MostMs(), "ms");
    Published.Places("structure posting time, most", Cost.BakePosting.MostMs(), "ms");
    Published.Places("structure candidate selection time, most",
                     World.StructureBuilds.SlowestCandidateSelectionMs(),
                     "ms");
    Published.Places("structure height resolution time, most",
                     World.StructureBuilds.SlowestHeightResolutionMs(),
                     "ms");
    Published.Places("structure raw extraction time, most",
                     World.StructureBuilds.SlowestRawExtractionMs(),
                     "ms");
    Published.Places(
        "structure task posting time, most", World.StructureBuilds.SlowestTaskPostingMs(), "ms");
    Published.Places("world growth time, most", Cost.Growth.MostMs(), "ms");
    Published.Places("simulation core time, most", Cost.Simulation.MostMs(), "ms");
    Published.Places("ground candidate time, most", Cost.Ground.MostMs(), "ms");
    Published.Places("ground request time, most", Cost.GroundRequest.MostMs(), "ms");
    Published.Places("ground candidate begin time, most", Cost.GroundBuildBegin.MostMs(), "ms");
    Published.Places("ground candidate creation time, most", Cost.GroundBuildCreate.MostMs(), "ms");
    Published.Places(
        "ground candidate preparation time, most", Cost.GroundBuildPrepare.MostMs(), "ms");
    static constexpr std::array<std::string_view, Spent::kGroundPhaseCount> kGroundPhases{
        "candidate",
        "patchwork",
        "sheet fields",
        "sheet refinement",
        "sheet halos",
        "sheet mesh",
        "classes",
        "surface",
        "models",
        "network",
        "structure bake",
        "corridors",
        "earthworks",
        "terrain mesh",
        "water",
        "geometry",
        "publication"};
    for (size_t phase = 0; phase < Cost.GroundPhases.size(); ++phase) {
      if (Cost.GroundPhases[phase].Taken() == 0) { continue; }
      Published.Places(std::string("ground phase ") + std::string(kGroundPhases[phase]) +
                           " time, most",
                       Cost.GroundPhases[phase].MostMs(),
                       "ms");
    }
    Published.Places("vegetation update time, most", Cost.Crowns.MostMs(), "ms");
  }
  if (Picture.Standing) {
    for (size_t at = 0; at < Render::kStageCount; ++at) {
      const auto stage = static_cast<Render::Stage>(at);
      const Render::SceneRenderer::Effort &spent = Picture.Device.Spent(stage);
      if (spent.TookMs <= 0.0 && spent.Draws == 0) { continue; }
      Published.Places(std::string(Row(stage).Name) + ", took", spent.TookMs, "ms");
      Published.Places(
          std::string(Row(stage).Name) + ", drew", static_cast<double>(spent.Draws), "draws");
      Published.Places(std::string(Row(stage).Name) + ", triangles",
                       static_cast<double>(spent.Triangles),
                       "triangles");
      Published.Places(std::string(Row(stage).Name) + ", surfaces",
                       static_cast<double>(spent.Surfaces),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", placements",
                       static_cast<double>(spent.Placements),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", textured",
                       static_cast<double>(spent.Textured),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", colour images",
                       static_cast<double>(spent.Palettes),
                       "images");
      Published.Places(std::string(Row(stage).Name) + ", device bytes",
                       static_cast<double>(spent.DeviceBytes),
                       "bytes");
      Published.Places(std::string(Row(stage).Name) + ", placements that differ",
                       static_cast<double>(spent.Distinct),
                       "rows");
      Published.Places(std::string(Row(stage).Name) + ", vertex layouts",
                       static_cast<double>(spent.Layouts),
                       "layouts");
    }
  }
  if (Cost.Render.Taken() > 0) {
    Published.Places("the picture's own time, last", Cost.Render.LastMs(), "ms");
    Published.Places("the picture's own time, least", Cost.Render.LeastMs(), "ms");
    Published.Places("the picture's own time, most", Cost.Render.MostMs(), "ms");
    Published.Places("pictures drawn", static_cast<double>(Cost.Render.Taken()), "pictures");
  }
  {
    const std::vector<std::string> clashed = Published.Clashed();
    Published.Places(
        "measures published twice in one round", static_cast<double>(clashed.size()), "rows");
    for (const std::string &one : clashed) {
      Published.Places("published twice in one round: " + one, 1.0, "rows");
    }
  }

  PublishAudioSnapshot();
}

void Engine::State::PublishAudioSnapshot() {
  const unsigned next = (Session.Told.load(std::memory_order_relaxed) + 1u) & 1u;
  std::vector<Audio::Heard> &sources = Session.Sources[next];
  sources.clear();
  sources.reserve(Session.Declared.Sounds.size());
  for (size_t source = 0; source < Session.Declared.Sounds.size(); ++source) {
    const Audio::SoundSource &declared = Session.Declared.Sounds[source];
    Audio::Heard where;
    where.Id = declared.Id;
    if (declared.Body.empty()) {
      where.Standing = !declared.Spatial.Positional;
      sources.push_back(where);
      continue;
    }
    const Physics::Rigid *stood = nullptr;
    const auto binding =
        source < Session.AudioBodies.size() ? Session.AudioBodies[source] : std::nullopt;
    if (binding && Simulation->DeclarationRevision == Session.DeclarationRevision) {
      const auto &body = Simulation->DynamicBodies[*binding];
      if (Simulation->Entities.alive(body.Owner)) { stood = &body.Motion; }
    }
    if (stood != nullptr) {
      where.Standing = true;
      for (int axis = 0; axis < 3; ++axis) {
        where.AtM[axis] = stood->PositionM[axis];
        where.VelocityMs[axis] = stood->VelocityMs[axis];
      }
      where.Blocked = IsAudioOccluded(where.AtM) ? 1.0 : 0.0;
    }
    sources.push_back(where);
  }

  Audio::Listening &ear = Session.Ear[next];
  ear = Audio::Listening{};
  if (Picture.Standing) {
    const Render::Viewpoint &eye = Picture.Standing->Aimed();
    for (int axis = 0; axis < 3; ++axis) {
      ear.AtM[axis] = eye.EyeM[axis];
      ear.ForwardXyz[axis] = eye.Forward[axis];
      ear.RightXyz[axis] = eye.Right[axis];
    }
  }
  Session.Told.store(next, std::memory_order_release);
}

bool Engine::State::IsAudioOccluded(const Vec3 &sourceM) const {
  if (World.AudioOcclusion.Empty() || !Picture.Standing) { return false; }
  const Render::Viewpoint &eye = Picture.Standing->Aimed();
  Vec3f fromM;
  Vec3f along;
  double awayM = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double step = sourceM[axis] - eye.EyeM[axis];
    awayM += step * step;
  }
  awayM = std::sqrt(awayM);
  if (!(awayM > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) {
    fromM[axis] = static_cast<float>(eye.EyeM[axis]);
    along[axis] = static_cast<float>((sourceM[axis] - eye.EyeM[axis]) / awayM);
  }
  return World.AudioOcclusion.Occludes(
      {.OriginM = fromM, .Toward = along}, kNearestOccluderM, static_cast<float>(awayM));
}

}
