#ifndef DRAWSINK_H
#define DRAWSINK_H

#include "BodyId.h"
#include "ClusterId.h"

namespace outshine::Generators {

class DrawSink {
public:
  virtual ~DrawSink() = default;

  [[nodiscard]] virtual bool Add(BodyId body, ClusterId cluster,
                                 const Instance &instance) noexcept = 0;
  [[nodiscard]] virtual bool Full() const noexcept = 0;
};

}
#endif
