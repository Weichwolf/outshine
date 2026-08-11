#ifndef DRAWSINK_H
#define DRAWSINK_H

#include "BodyId.h"
#include "ClusterId.h"

namespace outshine::Generators {

class DrawSink {
public:
  virtual ~DrawSink() = default;

  /* The body is the argument that cannot be invented: a BodyId exists only where occupancy claimed
   * the space, so a thing drawn where nothing stands has no spelling. */
  [[nodiscard]] virtual bool Add(BodyId body, ClusterId cluster,
                                 const Instance &instance) noexcept = 0;
  virtual bool Full() const noexcept = 0;
};

} // namespace outshine::Generators
#endif
