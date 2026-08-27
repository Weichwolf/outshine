#include "GroundSupport.h"

namespace outshine::Sim {

void GroundSupport::Restand() { Held_ = Stack_.Classes().Read(); }

Underneath GroundSupport::At(double lat, double lon) const {
  Underneath out;
  const GroundSample sample = Stack_.Ground().Resident(lat, lon);
  if (!sample.TryAslM(&out.HeightAslM)) { return out; }
  for (int axis = 0; axis < 3; ++axis) { out.NormalM[axis] = sample.NormalM()[axis]; }
  double edgeM = 0.0;
  int runnerUp = -1;
  const int row =
      Held_ ? Stack_.Classes().ClassAt(*Held_, lat, lon, &edgeM, &runnerUp) : -1;
  const size_t tpl = row >= 0 ? (size_t)row : (size_t)Templates_.UnmappedRow();
  out.Friction = (double)Templates_.FrictionOf(tpl);
  out.Known = out.Friction > 0.0;
  return out;
}

}
