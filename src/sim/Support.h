#ifndef OUTSHINE_SIM_UNDERFOOT_H
#define OUTSHINE_SIM_UNDERFOOT_H

namespace outshine::Sim {

struct Underneath {
  bool Known = false;
  double HeightAslM = 0.0;
  double NormalM[3] = {0.0, 1.0, 0.0};
  double Friction = 0.0;
};

class Support {
public:
  virtual ~Support() = default;
  [[nodiscard]] virtual Underneath At(double lat, double lon) const = 0;
};

}

#endif
