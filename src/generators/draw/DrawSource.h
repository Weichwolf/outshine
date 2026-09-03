#ifndef OUTSHINE_GENERATORS_DRAW_DRAWSOURCE_H
#define OUTSHINE_GENERATORS_DRAW_DRAWSOURCE_H

#include <span>
#include "BodyId.h"
#include "ContactMaterial.h"
#include "DrawSink.h"
#include "Ground.h"

namespace outshine::Generators {

class DrawSource {
public:
  virtual ~DrawSource() = default;
  DrawSource(const DrawSource &) = delete;
  DrawSource &operator=(const DrawSource &) = delete;

  virtual void Draw(const Ground &ground,
                    std::span<const Solid> placed,
                    BodyRange mine,
                    DrawSink &sink) const noexcept = 0;

protected:
  DrawSource() = default;
};

} // namespace outshine::Generators
#endif
