#ifndef OUTSHINE_WORLD_GENERATORS_DRAW_FORESTDRAW_H
#define OUTSHINE_WORLD_GENERATORS_DRAW_FORESTDRAW_H

#include "ClusterId.h"
#include "DrawSource.h"

namespace outshine::Generators {

class ForestDraw : public DrawSource {
public:

  ForestDraw(ClusterId cluster, double heightM) : Cluster_(cluster), HeightM_(heightM) {}

  void Draw(const Ground &ground, Span<const Body> placed, DrawSink &sink) const noexcept override;

private:
  ClusterId Cluster_;
  double HeightM_;
};

}
#endif
