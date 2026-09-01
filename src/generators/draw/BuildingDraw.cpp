#include "BuildingDraw.h"

namespace outshine::Generators {

void BuildingDraw::Draw(const Ground &ground,
                        Span<const Body> placed,
                        DrawSink &sink) const noexcept {
  (void)ground;
  if (!(HeightM_ > 0.0)) { return; }
  for (const Body &body : placed) {
    const std::optional<BodyId> id = body.Id();
    if (!id) { continue; }
    if (!(body.HeightM > 0.0f)) { continue; }
    Instance instance;
    instance.Em = static_cast<float>(body.Em);
    instance.Nm = static_cast<float>(body.Nm);
    instance.AslM = static_cast<float>(body.BaseAslM);
    instance.YawRad = body.YawRad;
    instance.Scale = static_cast<float>(static_cast<double>(body.HeightM) / HeightM_);
    if (!sink.Add(*id, Cluster_, instance)) { return; }
  }
}

} // namespace outshine::Generators
