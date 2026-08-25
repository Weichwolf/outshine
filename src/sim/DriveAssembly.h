#ifndef OUTSHINE_SIM_DRIVEASSEMBLY_H
#define OUTSHINE_SIM_DRIVEASSEMBLY_H

#include <string>

#include "Assembled.h"
#include "Column.h"
#include <Scenario.h>
#include "Store.h"

#include "CorridorLay.h"
#include "DriveTick.h"
#include "Rigging.h"
#include "Sink.h"

namespace outshine::Data {
class Transport;
}
namespace outshine::Ground {
class GroundStack;
}

namespace outshine::Sim {

struct Provision {
  std::string CacheDir;
  std::string AssetsDir;
  std::vector<Provider> Providers;
};

struct Harvest {
  long Features = 0;
  long Ways = 0;
  long NotACarriageway = 0;
  long TooNarrow = 0;
  long Ungraded = 0;
  long Nodes = 0;
  long Junctions = 0;
  long Crossings = 0;
  long TurnsRefused = 0;
  double WidestRefusedM = 0.0;
  double FetchedS = 0.0;
  double RouteLengthM = 0.0;
  double StraightM = 0.0;
  double FromAwayM = 0.0;
  double ToAwayM = 0.0;
  bool StreetsAbsent = false;
  bool RanOutOfPatience = false;
};

struct DriveProduct {
  Vehicle Car;
  Rigged Stood;
  Corridor Way;
  DriveState State;
  Harvest Found;
  bool Ready = false;
};

[[nodiscard]] bool AssembleDrive(const Store &scene, const Assembled &cast,
                                 const Column<Vehicle> &vehicles, const Column<Drive> &driven,
                                 const WorldSettings &world, Ground::GroundStack &stack,
                                 Data::Transport &wire, const Provision &kept, Sink &say,
                                 DriveProduct &out);

}

#endif
