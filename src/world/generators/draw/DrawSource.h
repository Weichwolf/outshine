#ifndef OUTSHINE_WORLD_GENERATORS_DRAW_DRAWSOURCE_H
#define OUTSHINE_WORLD_GENERATORS_DRAW_DRAWSOURCE_H

#include "ContactMaterial.h"
#include "DrawSink.h"
#include "Ground.h"
#include "Span.h"

namespace outshine::Generators {

class DrawSource {
public:
  virtual ~DrawSource() = default;
  DrawSource(const DrawSource &) = delete;
  DrawSource &operator=(const DrawSource &) = delete;

  virtual void Draw(const Ground &ground, Span<const Body> placed,
                    DrawSink &sink) const noexcept = 0;

protected:
  DrawSource() = default;
};

}
#endif
