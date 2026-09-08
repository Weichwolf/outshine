#ifndef OUTSHINE_GENERATORS_FLORA_FORESTDRAW_H
#define OUTSHINE_GENERATORS_FLORA_FORESTDRAW_H

#include <span>
#include <vector>
#include "ClusterId.h"
#include "DrawSource.h"

namespace outshine::Generators {

class ForestDraw : public DrawSource {
public:
  struct Prototype {
    ClusterId Cluster;
    double HeightM;
  };

  explicit ForestDraw(std::span<const Prototype> prototypes)
      : Prototypes_(prototypes.begin(), prototypes.end()) {}

  void Draw(const Ground &ground,
            std::span<const Solid> placed,
            BodyRange mine,
            DrawSink &sink) const noexcept override;

private:
  std::vector<Prototype> Prototypes_;
};

}
#endif
