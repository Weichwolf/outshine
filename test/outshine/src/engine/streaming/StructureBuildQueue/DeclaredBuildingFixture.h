#ifndef OUTSHINE_TEST_STRUCTUREBUILDQUEUE_DECLAREDBUILDINGFIXTURE_H
#define OUTSHINE_TEST_STRUCTUREBUILDQUEUE_DECLAREDBUILDINGFIXTURE_H

#include "Check.h"
#include "SurfacePreparation.h"
#include "OfflineTransport.h"
#include "Sink.h"
#include "Tasks.h"
#include <SourceProvider.h>
#include "OsmField.h"

#include <array>
#include <string>

namespace outshine::Test {
inline auto DeclaredBuildings() {
  constexpr double kSouthDeg = 49.32739;
  constexpr double kNorthDeg = 49.32741;
  constexpr double kWestDeg = 8.56589;
  constexpr double kEastDeg = 8.56591;
  return std::array<::outshine::Generators::Osm::OsmField::Declared, 1>{{
      {.Layer = "buildings",
       .Key = "kind",
       .Value = "building",
       .HeightM = 12,
       .Area = true,
       .LatLon =
           {kSouthDeg, kWestDeg, kSouthDeg, kEastDeg, kNorthDeg, kEastDeg, kNorthDeg, kWestDeg}},
  }};
}

inline bool OpenDeclaredBuilding(Ground::SurfacePreparation &stack,
                                 const std::string &cache,
                                 LongitudeLatitude eye,
                                 Data::OfflineTransport &wire,
                                 Tasks &sourceCompute,
                                 Sink &sink) {
  using namespace Ground;
  const std::array providers{Data::SourceProvider{.Kind = "terrain",
                                                  .Revision = {},
                                                  .Dataset = {},
                                                  .Location = {},
                                                  .Endpoint = {},
                                                  .Coverage = {},
                                                  .PayloadSha256 = {},
                                                  .Schema = {}}};
  CHECK(stack.Open({.Shipped = "data", .Cache = cache, .AssetCache = {}},
                   providers,
                   eye,
                   wire,
                   sourceCompute,
                   sink,
                   nullptr,
                   1.0),
        "offline stack opens");
  if (!stack.Opened()) { return false; }
  const auto buildings = DeclaredBuildings();
  CHECK(stack.Declares(buildings), "declared source snapshot is accepted");
  CHECK(stack.AdvanceAt(eye, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
        "declared building enters the vector snapshot");
  const ::outshine::Generators::Osm::OsmField *vectors = stack.Vectors();
  CHECK(vectors && vectors->Tiles().size() == 1 && !vectors->Rings().empty(),
        "one building tile is available");
  return vectors != nullptr && vectors->Tiles().size() == 1 && !vectors->Rings().empty();
}
}

#endif
