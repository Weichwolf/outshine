#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSHAPE_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSHAPE_H

#include <span>
#include <expected>
#include "Earth.h"
#include <cstdint>
#include <vector>

#include "StructureMesher.h"

namespace outshine::Generators {

constexpr double kFloorUnsaidM = 2.9;

enum class RoofKind : uint8_t { Flat, Gable, Hip, Shed, Mansard, Sawtooth, Dome };

enum class BuildingUse : uint8_t { Outbuilding, House, Terrace, Block, Hall, Tower, Spire };

struct Boxed {
  double U = 0.0;
  double V = 0.0;
};

struct BuildingShape {
  std::vector<EastNorth> Ring;
  size_t TidiedAway = 0;

  std::vector<uint8_t> PartyWallEdges;
  double AreaM2 = 0.0;
  EastNorth Centre;
  EastNorth AxisU;
  double HalfUm = 0.0, HalfVm = 0.0;
  double Fill = 0.0;

  BuildingUse Use = BuildingUse::House;
  RoofKind Roof = RoofKind::Flat;
  int Storeys = 1;
  double FloorM = kFloorUnsaidM;
  double FootM = 0.0;
  double SeatM = 0.0;
  double SoleM = 0.0;
  double EavesM = 0.0;
  double RiseM = 0.0;
  double BreakFracV = 0.0;
  double BreakRiseM = 0.0;
  double PeriodM = 0.0;
  double BayM = 3.0;
  double OverhangM = 0.0;
  uint32_t Seed = 0;
  int Ident = 0;
  int FrontEdge = -1;

  [[nodiscard]] bool Valid() const { return Ring.size() >= 3 && AreaM2 > 1.0; }

  [[nodiscard]] bool OnGround() const { return FootM <= 0.0; }

  [[nodiscard]] double TopM() const { return SeatM + FootM + EavesM + RiseM; }

  [[nodiscard]] EastNorth AxisV() const { return {.EastM = -AxisU.NorthM, .NorthM = AxisU.EastM}; }

  [[nodiscard]] Boxed ToBox(const EastNorth &p) const;
  [[nodiscard]] EastNorth FromBox(Boxed at) const;
};

struct BuildingScratch;

struct Order {
  double HeightM = 0.0;
  bool HeightMeasured = false;
  double PitchedShare = -1.0;
};

[[nodiscard]] std::expected<std::span<BuildingShape>, StructureMeshError>
MassOf(std::span<const double> ringLatLon,
       Order order,
       const Frontage &street,
       BuildingScratch &scratch);

}
#endif
