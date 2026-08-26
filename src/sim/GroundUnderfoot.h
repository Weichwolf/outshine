#ifndef OUTSHINE_SIM_GROUNDUNDERFOOT_H
#define OUTSHINE_SIM_GROUNDUNDERFOOT_H

#include "GroundStack.h"
#include "Underfoot.h"
#include "VegetationTemplates.h"

namespace outshine::Sim {

class GroundUnderfoot final : public Underfoot {
public:
  GroundUnderfoot(const Ground::GroundStack &stack, const Ground::VegetationTemplates &templates)
      : Stack_(stack), Templates_(templates) {}

  [[nodiscard]] Standing At(double lat, double lon) const override;
  [[nodiscard]] double PostM(double lat) const override;

private:
  const Ground::GroundStack &Stack_;
  const Ground::VegetationTemplates &Templates_;
};

}

#endif
