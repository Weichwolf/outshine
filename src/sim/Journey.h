#ifndef OUTSHINE_SIM_JOURNEY_H
#define OUTSHINE_SIM_JOURNEY_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <outshine/Assembled.h>
#include <outshine/Column.h>
#include <outshine/Scenario.h>
#include <outshine/Store.h>

#include "Body.h"
#include "DriveTick.h"
#include "Sink.h"
#include "TerrainLoader.h"
#include "Fit.h"
#include "ReferenceLine.h"
#include "Rig.h"
#include "SpeedProfile.h"
#include "Wayfinding.h"

namespace outshine::Data {
class Transport;
}

namespace outshine::Sim {

struct Provision {
  std::string CacheDir;
  std::string AssetsDir;
};



class Journey {
public:
  Journey();
  ~Journey();
  Journey(const Journey &) = delete;
  Journey &operator=(const Journey &) = delete;

  [[nodiscard]] bool Lay(const Store &scene, const Assembled &cast,
                         const Column<Vehicle> &vehicles, const Column<Drive> &driven,
                         const WorldSettings &world, Data::Transport &wire,
                         const Provision &kept, Sink &say);
  [[nodiscard]] Ridden Ride(double dtS, const Taken *taken = nullptr);
  void Close(void);

  [[nodiscard]] const Physics::Body &Carried(void) const;
  [[nodiscard]] const ReferenceLine &Corridor(void) const;
  [[nodiscard]] World::GroundStream &Ground(void) const;
  [[nodiscard]] double LengthM(void) const;
  [[nodiscard]] double ReserveMs2(void) const;
  void Frame(double &latDeg, double &lonDeg, double &perLatM, double &perLonM) const;

private:
  struct State;
  std::unique_ptr<State> S_;
};

} // namespace outshine::Sim

#endif
