#ifndef OUTSHINE_SIM_GROUNDSUPPORT_H
#define OUTSHINE_SIM_GROUNDSUPPORT_H

#include <memory>

#include "ClassStructure.h"
#include "GroundStack.h"
#include "Support.h"
#include "VegetationTemplates.h"

namespace outshine::Sim {

class GroundSupport final : public Support {
public:
  GroundSupport(const Ground::GroundStack &stack, const Ground::VegetationTemplates &templates)
      : Stack_(stack), Templates_(templates) {}

  [[nodiscard]] Underneath At(double lat, double lon) const override;
  void Restand();

private:
  const Ground::GroundStack &Stack_;
  const Ground::VegetationTemplates &Templates_;
  std::shared_ptr<const ClassStructure> Held_;
};

} // namespace outshine::Sim

#endif
