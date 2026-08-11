/* THE FOREST'S PICTURE HALF: one instance of one grown cluster per stand that claimed space. It
 * invents nothing — every number it emits is read off a body, which is why a tree can only be drawn
 * where one stands. */
#ifndef FORESTDRAW_H
#define FORESTDRAW_H

#include "ClusterId.h"
#include "DrawSource.h"

namespace outshine::Generators {

class ForestDraw : public DrawSource {
public:
  /* `heightM` is the prototype's own height in metres: it is what turns a body's height into the
   * factor the cluster is drawn at. */
  ForestDraw(ClusterId cluster, double heightM) : Cluster_(cluster), HeightM_(heightM) {}

  void Draw(const Ground &ground, Span<const Body> placed, DrawSink &sink) const noexcept override;

private:
  ClusterId Cluster_;
  double HeightM_;
};

} // namespace outshine::Generators
#endif
