#ifndef DRAWSOURCE_H
#define DRAWSOURCE_H

#include "Body.h"
#include "DrawSink.h"
#include "Ground.h"
#include "Span.h"

namespace outshine::Generators {

class DrawSource {
public:
  virtual ~DrawSource() = default;
  DrawSource(const DrawSource &) = delete;
  DrawSource &operator=(const DrawSource &) = delete;

  /* `placed` is what occupancy actually claimed for this source, in the order it claimed it. The
   * region is not a parameter here either — the ground is of exactly one and says which. */
  virtual void Draw(const Ground &ground, Span<const Body> placed,
                    DrawSink &sink) const noexcept = 0;

protected:
  DrawSource() = default;
};

} // namespace outshine::Generators
#endif
