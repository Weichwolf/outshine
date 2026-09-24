#include "EngineHeld.h"
#include "math/Vec3.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace outshine {

constexpr float kNearestOccluderM = 0.01f;

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
