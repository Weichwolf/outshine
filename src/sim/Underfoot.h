#ifndef OUTSHINE_SIM_UNDERFOOT_H
#define OUTSHINE_SIM_UNDERFOOT_H

namespace outshine::Sim {

struct Standing {
  bool Known = false;
  double HeightAslM = 0.0;
  double Friction = 0.0;
};

class Underfoot {
public:
  virtual ~Underfoot() = default;
  [[nodiscard]] virtual Standing At(double lat, double lon) const = 0;
  [[nodiscard]] virtual double PostM(double lat) const = 0;
};

}

#endif
