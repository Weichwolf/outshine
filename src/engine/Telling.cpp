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

constexpr float kNearestOccluderM = 0.01f;

bool Engine::State::Stood() {
  if (Picture.Standing) { return true; }
  if (!Picture.Targeted) {
    Error = "no canvas stands, so there is nowhere to draw -- the client hands one in through "
            "DrawsInto";
    return false;
  }
  Core::Declaration wanted = Picture.Shown;
  wanted.SurfaceWidthPx = Picture.Frame.WidthPx;
  wanted.SurfaceHeightPx = Picture.Frame.HeightPx;
  if (!Core::Live::Open(
          Picture.Device, std::move(wanted), &Picture.Face, Picture.Standing, Error)) {
    return false;
  }
  if (!Picture.Carrying) { return true; }
  Picture.Carrying = false;
  return Picture.Standing->Restand(Picture.Handed, 0, Error);
}

void Engine::State::Blocks(const Gltf::Subject &standing) {
  const std::vector<double> &positionsM = standing.PositionsM();
  std::vector<float> corners(positionsM.size());
  for (size_t at = 0; at < positionsM.size(); ++at) {
    corners[at] = static_cast<float>(positionsM[at]);
  }
  World.Blocking = TriangleBvh::Over(
      std::span<const float>(corners.data(), corners.size()),
      std::span<const uint32_t>(standing.Indices().data(), standing.Indices().size()));
}

void Engine::State::Tells() {
  const Heap::Tagged telling("frame-tells");
  Published.Places("heap: bytes LIVE right now", static_cast<double>(Heap::LiveBytes()), "bytes");
  for (size_t at = 0; at < Heap::TagCount(); ++at) {
    const char *const tag = Heap::TagAt(at);
    if (tag == nullptr || Heap::TakenAt(at) == 0) { continue; }
    Published.Places(
        std::string("heap taken under ") + tag, static_cast<double>(Heap::TakenAt(at)), "bytes");
  }
  if (Cost.Advance.Taken() > 0) {
    Published.Places("the step's own time, last", Cost.Advance.LastMs(), "ms");
    Published.Places("the step's own time, least", Cost.Advance.LeastMs(), "ms");
    Published.Places("the step's own time, most", Cost.Advance.MostMs(), "ms");
    Published.Places("steps taken", static_cast<double>(Cost.Advance.Taken()), "steps");
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

  const unsigned next = (Session.Told.load(std::memory_order_relaxed) + 1u) & 1u;
  std::vector<Audio::Heard> &sources = Session.Sources[next];
  sources.clear();
  sources.reserve(Session.Declared.Sounds.size());
  for (const Scenario::Sound &declared : Session.Declared.Sounds) {
    Audio::Heard where;
    where.Id = declared.Id;
    if (declared.On.empty()) {
      where.Standing = !declared.Heard.Positional;
      sources.push_back(where);
      continue;
    }
    const Physics::Rigid *stood = nullptr;
    if (Ticking.Drove && Session.Declared.Bodies.size() == 1) {
      stood = &Ticking.Drive.State.Body;
    } else if (!Ticking.Freestanding.empty()) {
      stood = &Ticking.Freestanding.front();
    }
    if (stood != nullptr) {
      where.Standing = true;
      for (int axis = 0; axis < 3; ++axis) {
        where.AtM[axis] = stood->PositionM[axis];
        where.VelocityMs[axis] = stood->VelocityMs[axis];
      }
      where.Blocked = Blocked(where.AtM) ? 1.0 : 0.0;
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

bool Engine::State::Blocked(const Vec3 &sourceM) const {
  if (World.Blocking.Empty() || !Picture.Standing) { return false; }
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
  return World.Blocking.Occludes(
      {.OriginM = fromM, .Toward = along}, kNearestOccluderM, static_cast<float>(awayM));
}

} // namespace outshine
