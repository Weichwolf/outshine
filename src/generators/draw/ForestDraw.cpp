#include "ForestDraw.h"

namespace outshine::Generators {

void ForestDraw::Draw(const Ground &ground,
                      Span<const Body> placed,
                      BodyRange mine,
                      DrawSink &sink) const noexcept {
  (void)ground;
  if (!(HeightM_ > 0.0)) { return; }
  for (uint32_t at = 0; at < mine.Count; ++at) {
    const Body &body = placed[at];
    Instance instance;
    instance.Em = static_cast<float>(body.Em);
    instance.Nm = static_cast<float>(body.Nm);
    instance.AslM = static_cast<float>(body.BaseAslM);
    instance.YawRad = body.YawRad;
    instance.Scale = static_cast<float>(static_cast<double>(body.HeightM) / HeightM_);
    if (!sink.Add(mine.Nth(at), Cluster_, instance)) { return; }
  }
}

} // namespace outshine::Generators
