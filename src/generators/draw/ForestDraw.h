#ifndef OUTSHINE_GENERATORS_DRAW_FORESTDRAW_H
#define OUTSHINE_GENERATORS_DRAW_FORESTDRAW_H

#include <span>
#include "ClusterId.h"
#include "DrawSource.h"

namespace outshine::Generators {

class ForestDraw : public DrawSource {
public:
  ForestDraw(ClusterId cluster, double heightM) : Cluster_(cluster), HeightM_(heightM) {}

  void Draw(const Ground &ground,
            std::span<const Body> placed,
            BodyRange mine,
            DrawSink &sink) const noexcept override;

private:
  ClusterId Cluster_;
  double HeightM_;
};

} // namespace outshine::Generators
#endif
