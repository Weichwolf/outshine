#ifndef OUTSHINE_CLIENTS_SCENEWEATHER_H
#define OUTSHINE_CLIENTS_SCENEWEATHER_H

#include "ConstantWindWeather.h"
#include "Stage.h"
#include "Units.h"

namespace outshine::Clients {

class SceneWeather : public ConstantWindWeather {
public:

  explicit SceneWeather(const SceneLegacy::WorldStage *world)
      : ConstantWindWeather(world ? world->WindFromDeg : 0.0,
                            world ? world->WindMs / kKtToMs : 0.0),
        Cover_(world ? world->CloudCover : 0.0) {}

  CloudLayers Clouds(double latDeg, double lonDeg) const override {
    (void)latDeg; (void)lonDeg;
    CloudLayers c;
    c.TotalPct = c.LowPct = Cover_ * 100.0;
    return c;
  }

private:
  double Cover_;
};

}
#endif
