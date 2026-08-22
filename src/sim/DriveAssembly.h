#ifndef OUTSHINE_SIM_DRIVEASSEMBLY_H
#define OUTSHINE_SIM_DRIVEASSEMBLY_H

#include <string>

#include <outshine/Assembled.h>
#include <outshine/Column.h>
#include <outshine/Scenario.h>
#include <outshine/Store.h>

#include "CorridorLay.h"
#include "DriveTick.h"
#include "Rigging.h"
#include "Sink.h"

namespace outshine::Data {
class Transport;
}
namespace outshine::World {
class GroundStack;
}

namespace outshine::Sim {

struct Provision {
  std::string CacheDir;
  std::string AssetsDir;
};

struct DriveProduct {
  Vehicle Car;
  Rigged Stood;
  Corridor Way;
  DriveState State;
  bool Ready = false;
};

[[nodiscard]] bool AssembleDrive(const Store &scene, const Assembled &cast,
                                 const Column<Vehicle> &vehicles, const Column<Drive> &driven,
                                 const WorldSettings &world, World::GroundStack &stack,
                                 Data::Transport &wire, const Provision &kept, Sink &say,
                                 DriveProduct &out);

}

#endif
