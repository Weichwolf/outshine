#include "ForestDraw.h"

namespace outshine::Generators {

void ForestDraw::Draw(const Ground &ground,
                      Span<const Body> placed,
                      DrawSink &sink) const noexcept {
  (void)ground;
  if (!(HeightM_ > 0.0)) { return; }
  for (const Body &body : placed) {
    const std::optional<BodyId> id = body.Id();
    if (!id) { continue; }
    Instance instance;
    instance.Em = (float)body.Em;
    instance.Nm = (float)body.Nm;
    instance.AslM = (float)body.BaseAslM;
    instance.YawRad = body.YawRad;
    instance.Scale = (float)((double)body.HeightM / HeightM_);
    if (!sink.Add(*id, Cluster_, instance)) { return; }
  }
}

}
