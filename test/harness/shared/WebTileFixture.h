#ifndef OUTSHINE_TEST_WEBTILEFIXTURE_H
#define OUTSHINE_TEST_WEBTILEFIXTURE_H

#include "WebTileSource.h"
#include <string>
#include <utility>

namespace outshine::Test {

class WebTileFixture final : public Data::WebTileSource {
public:
  WebTileFixture(std::string revision,
                 Data::Rank order,
                 Data::AbsencePolicy absence,
                 std::string dataset = "tile-fixture",
                 std::string endpoint = "https://fixture.invalid/{z}/{x}/{y}.png")
      : WebTileSource(Data::SourceDecl{.Id = std::move(dataset),
                                       .Revision = std::move(revision),
                                       .Endpoint = endpoint,
                                       .Kind = Data::DataKind::Elevation,
                                       .How = Data::Scheme::TileZxy,
                                       .Wire = Data::WireFormat::TerrariumPng,
                                       .Order = order,
                                       .OnAbsent = absence,
                                       .MinZoom = 0,
                                       .MaxZoom = 15,
                                       .AncestorFill = true,
                                       .Keeps = Data::Cacheability::Forever,
                                       .Need = Data::Necessity::Required,
                                       .Latency = Data::LatencyClass::Distant,
                                       .RetryBudget = 4},
                      endpoint) {}
};

}
#endif
