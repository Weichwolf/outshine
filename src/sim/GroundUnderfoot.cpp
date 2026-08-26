#include "GroundUnderfoot.h"

namespace outshine::Sim {

Standing GroundUnderfoot::At(double lat, double lon) const {
  Standing out;
  if (!Stack_.Ground().At(lat, lon).TryAslM(&out.HeightAslM)) { return out; }
  double edgeM = 0.0;
  int runnerUp = -1;
  const int row = Stack_.Classes().ClassAt(lat, lon, &edgeM, &runnerUp);
  const size_t tpl = row >= 0 ? (size_t)row : (size_t)Templates_.UnmappedRow();
  out.Friction = (double)Templates_.FrictionOf(tpl);
  out.Known = out.Friction > 0.0;
  return out;
}

double GroundUnderfoot::PostM(double lat) const { return Stack_.Ground().PostM(lat); }

}
