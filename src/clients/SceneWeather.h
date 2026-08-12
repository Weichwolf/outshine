/* A DECLARED WORLD STAGE AS WEATHER. A scene declares one wind and one cloud cover, which is less
 * than a GFS sample carries, so the widening happens once and here rather than in each client. */
#ifndef SCENEWEATHER_H
#define SCENEWEATHER_H

#include "ConstantWindWeather.h"
#include "Stage.h"
#include "Units.h"

namespace outshine::Clients {

class SceneWeather : public ConstantWindWeather {
public:
  /* A STUDIO HAS NO MET WIND AND NO SKY TO COVER, so the absent world arm is calm and clear — which
   * is what a still life is judged in, and what the bench then overrides per shot. */
  explicit SceneWeather(const Scenario::WorldStage *world)
      : ConstantWindWeather(world ? world->WindFromDeg : 0.0,
                            world ? world->WindMs / kKtToMs : 0.0),
        Cover_(world ? world->CloudCover : 0.0) {}

  /* ONE declared cover, and it goes to the LOW deck [SET]. That deck is the one that shadows the
   * ground and the one a pedestrian sees the underside of; putting the same number on all three
   * would stack three independent fields into a lid with no holes in it, which is the opposite of
   * what "0.55" says. */
  CloudLayers Clouds(double latDeg, double lonDeg) const override {
    (void)latDeg; (void)lonDeg;
    CloudLayers c;
    c.TotalPct = c.LowPct = Cover_ * 100.0;
    return c;
  }

private:
  double Cover_;
};

} // namespace outshine::Clients
#endif
