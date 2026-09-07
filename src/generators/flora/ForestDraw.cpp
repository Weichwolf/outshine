#include <span>
#include "ForestDraw.h"
#include <cstdint>
#include <cassert>

namespace outshine::Generators {

void ForestDraw::Draw(const Ground &ground,
                      std::span<const Solid> placed,
                      BodyRange mine,
                      DrawSink &sink) const noexcept {
  (void)ground;
  for (uint32_t at = 0; at < mine.Count; ++at) {
    const Solid &body = placed[at];
    assert(body.Variant < Prototypes_.size());
    const Prototype &prototype = Prototypes_[body.Variant];
    assert(prototype.HeightM > 0.0);
    Scattered instance;
    instance.Em = body.Em;
    instance.Nm = body.Nm;
    instance.AslM = body.BaseAslM;
    instance.YawRad = body.YawRad;
    instance.Scale = static_cast<float>(static_cast<double>(body.HeightM) / prototype.HeightM);
    if (!sink.Add(mine.Nth(at), prototype.Cluster, instance)) { return; }
  }
}

} // namespace outshine::Generators
