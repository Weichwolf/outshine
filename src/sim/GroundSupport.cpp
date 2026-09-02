#include "GroundSupport.h"
#include <cstddef>
#include <optional>

namespace outshine::Sim {

void GroundSupport::Restand() {
  Held_ = Stack_.Classes().Read();
}

Underneath GroundSupport::At(double lat, double lon) const {
  Underneath out;
  const GroundSample sample = Stack_.Ground().Resident({.LongitudeDeg = lon, .LatitudeDeg = lat});
  const std::optional<double> aslM = sample.AslM();
  if (!aslM) { return out; }
  out.HeightAslM = *aslM;
  for (int axis = 0; axis < 3; ++axis) { out.NormalM[axis] = sample.NormalM()[axis]; }
  double edgeM = 0.0;
  int runnerUp = -1;
  const int row = Held_ ? Stack_.Classes().ClassAt(
                              *Held_, {.LongitudeDeg = lon, .LatitudeDeg = lat}, &edgeM, &runnerUp)
                        : -1;
  const size_t tpl =
      row >= 0 ? static_cast<size_t>(row) : static_cast<size_t>(Templates_.UnmappedRow());
  out.Friction = static_cast<double>(Templates_.FrictionOf(tpl));
  out.Known = out.Friction > 0.0;
  return out;
}

} // namespace outshine::Sim
