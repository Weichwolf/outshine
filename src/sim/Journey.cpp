#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Journey.h"

#include "Carriageway.h"
#include "Units.h"
#include "TileGeodesy.h"
#include "Ribbon.h"
#include "ContentStore.h"
#include "Drive.h"
#include "CorridorLay.h"
#include "Rigging.h"
#include "ScenarioRead.h"
#include "Fit.h"
#include "Transport.h"
#include "DeclaredSources.h"
#include "GroundMaterials.h"
#include "OsmField.h"
#include "RoadHarvest.h"
#include "SourceSet.h"
#include "TerrainLoader.h"
#include "GroundStack.h"
#include "TilePool.h"
#include "VegetationTemplates.h"
#include "Wayfinding.h"

using outshine::World::ApartM;
using outshine::World::Network;
using outshine::World::OsmField;
using outshine::World::OsmLayer;
using outshine::World::OsmLayerNames;
using outshine::World::Reap;
using outshine::World::Reaped;
using outshine::World::Route;
using outshine::World::TilePool;
using outshine::World::VegetationTemplates;
using outshine::World::Waypoint;

namespace {




} // namespace


namespace outshine::Sim {

struct Journey::State {
  outshine::World::GroundStack Stack;
  DriveProduct Product;
  bool Opened = false;
};

Journey::Journey() : S_(std::make_unique<State>()) {}
Journey::~Journey() { Close(); }

void Journey::Close(void) {
  if (S_ && S_->Opened) {
    S_->Stack.Close();
    S_->Opened = false;
  }
}

const outshine::Physics::Body &Journey::Carried(void) const { return S_->Product.State.Body; }
outshine::World::GroundStream &Journey::Ground(void) const { return S_->Stack.Ground(); }

const outshine::ReferenceLine &Journey::Corridor(void) const { return S_->Product.Way.Line; }
double Journey::LengthM(void) const { return S_->Product.Way.Line.LengthM(); }

double Journey::ReserveMs2(void) const { return S_->Product.Way.ReserveMs2; }

void Journey::Frame(double &latDeg, double &lonDeg, double &perLatM, double &perLonM) const {
  latDeg = S_->Product.Way.FrameLat;
  lonDeg = S_->Product.Way.FrameLon;
  perLatM = S_->Product.Way.PerLatM;
  perLonM = S_->Product.Way.PerLonM;
}

bool Journey::Lay(const Store &scene, const Assembled &cast, const Column<Vehicle> &vehicles,
                  const Column<Drive> &driven, const WorldSettings &world, Data::Transport &wire,
                  const Provision &kept, Sink &say) {
  S_->Opened = true;
  return AssembleDrive(scene, cast, vehicles, driven, world, S_->Stack, wire, kept, say,
                       S_->Product);
}

Ridden Journey::Ride(double dtS, const Taken *taken) {
  if (!S_->Product.Ready) {
    S_->Product.State.Tally.Found = false;
    return S_->Product.State.Tally;
  }
  return Sim::DriveTick(S_->Product.Way, S_->Product.Stood, S_->Product.Car, S_->Product.State,
                        dtS, taken);
}

} // namespace outshine::Sim
