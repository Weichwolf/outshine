#ifndef OUTSHINE_GENERATORS_DRAW_BUILDINGDRAW_H
#define OUTSHINE_GENERATORS_DRAW_BUILDINGDRAW_H

#include "DrawSource.h"

namespace outshine::Generators {

class BuildingDraw : public DrawSource {
public:
  BuildingDraw(ClusterId cluster, double heightM) : Cluster_(cluster), HeightM_(heightM) {}

  void Draw(const Ground &ground, Span<const Body> placed, DrawSink &sink) const noexcept override;

private:
  ClusterId Cluster_;
  double HeightM_;
};

}
#endif
