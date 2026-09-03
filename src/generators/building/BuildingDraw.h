#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGDRAW_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGDRAW_H

#include <span>
#include "DrawSource.h"

namespace outshine::Generators {

class BuildingDraw : public DrawSource {
public:
  BuildingDraw(ClusterId cluster, double heightM) : Cluster_(cluster), HeightM_(heightM) {}

  void Draw(const Ground &ground,
            std::span<const Solid> placed,
            BodyRange mine,
            DrawSink &sink) const noexcept override;

private:
  ClusterId Cluster_;
  double HeightM_;
};

} // namespace outshine::Generators
#endif
