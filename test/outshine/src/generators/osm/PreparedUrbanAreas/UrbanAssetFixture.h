#ifndef OUTSHINE_TEST_URBANASSETFIXTURE_H
#define OUTSHINE_TEST_URBANASSETFIXTURE_H

#include "PreparedUrbanAreas.h"
#include "SourceSet.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include "OsmCacheFixture.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace UrbanAssetFixture {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;
constexpr Data::TileId kOwner{.Zoom = 11, .X = 1023, .Y = 1024};
enum class Availability { Ready, Unavailable, Absent, ParentAbsent, Refused };

class NoTransport final : public SourceTransport {
public:
  FetchStart Begin(const std::string & /*url*/) override { return Ticket::None; }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Never(); }

  void Cancel(Ticket /*ticket*/) override {}
};

inline std::vector<uint8_t> UrbanTile() {
  using outshine::Test::Mvt::Append;
  using outshine::Test::Mvt::Bytes;
  constexpr uint8_t layerVersion = 0x78;
  constexpr uint8_t layerExtent = 0x28;
  constexpr uint8_t layerKey = 0x1a;
  constexpr uint8_t layerValue = 0x22;
  constexpr uint8_t featureType = 0x18;
  constexpr uint8_t featureTags = 0x12;
  constexpr uint8_t polygonGeometry = 0x22;
  constexpr uint8_t polygonLineTo = 26;
  Bytes layer{0x0a, 7, 'l', 'a', 'n', 'd', 'u', 's', 'e', layerVersion, 2, layerExtent, 32};
  Append(layer, layerKey, Bytes{'c', 'l', 'a', 's', 's'});
  Append(layer, layerValue, Bytes{0x0a, 11, 'r', 'e', 's', 'i', 'd', 'e', 'n', 't', 'i', 'a', 'l'});
  constexpr uint8_t negativeSide = 63;
  Bytes feature{0x08, 7, featureType, 3};
  Append(feature, featureTags, Bytes{0, 0});
  Append(
      feature, polygonGeometry, Bytes{9, 0, 0, polygonLineTo, 64, 0, 0, 64, negativeSide, 0, 15});
  Append(layer, featureTags, feature);
  Bytes tile;
  Append(tile, layerKey, layer);
  return tile;
}

class VectorSource final : public Source {
public:
  VectorSource(std::atomic_uint &calls, Availability availability)
      : Calls_(calls), Availability_(availability) {
    Decl_.Id = "urban-fixture";
    Decl_.Revision = "r1";
    Decl_.Kind = DataKind::VectorMap;
    Decl_.Wire = WireFormat::MapboxVectorTile;
    Decl_.Keeps = Cacheability::Never;
    Decl_.MaximumPayloadBytes = UrbanTile().size();
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const SourceRequest &request) const noexcept override {
    const auto tile = request.Where().Tile();
    constexpr int parentZoom = 5;
    return Availability_ == Availability::Absent ||
                   (Availability_ == Availability::ParentAbsent && tile && tile->Zoom == parentZoom)
               ? Coverage::Outside
               : Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*address*/, SourceTransport & /*transport*/) const override {
    return Ticket::None;
  }

  SourceResponse Collect(const Address & /*address*/,
                         Ticket /*ticket*/,
                         SourceTransport & /*transport*/) const override {
    ++Calls_;
    if (Availability_ == Availability::Refused) { return SourceResponse::Meant(Meaning::Refused); }
    return SourceResponse::Delivered(
        Availability_ == Availability::Unavailable ? std::vector<uint8_t>{} : UrbanTile());
  }

private:
  SourceDecl Decl_{};
  std::atomic_uint &Calls_;
  Availability Availability_ = Availability::Ready;
};

struct Fixture {
  std::atomic_uint Calls{0};
  std::shared_ptr<SourceSession> Session;
  NoTransport SourceTransport;
  Tasks Compute{1};
  std::shared_ptr<PreparedOsmTiles> Tiles;
  std::shared_ptr<PreparedUrbanAreas> Urban;

  Fixture(const std::filesystem::path &directory,
          Availability availability,
          PreparedUrbanAreas::Budget budget = {}) {
    auto session =
        outshine::Test::OsmCache::OpenSession(std::make_unique<VectorSource>(Calls, availability));
    CHECK(session, "fixture source session opens");
    if (!session) { return; }
    Session = std::move(*session);
    auto tiles = PreparedOsmTiles::Open(directory.string(), Session, SourceTransport, Compute);
    CHECK(tiles, "source-product cache opens");
    if (!tiles) { return; }
    Tiles = *tiles;
    auto urban =
        PreparedUrbanAreas::Open(directory.string(), Session->Sources(), Compute, Tiles, budget);
    CHECK(urban, "urban cache-first producer opens");
    if (urban) { Urban = *urban; }
  }
};

inline std::expected<PreparedUrbanAreas::SourceDelivery, std::string>
Await(Fixture &fixture, const PreparedUrbanAreas::Request &request) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  do {
    auto result = fixture.Urban->Acquire(request);
    if (!result || result->Status != PreparedUrbanAreas::State::Pending) { return result; }
    (void)fixture.Urban->AwaitProgress(
        std::chrono::duration<double>(std::chrono::milliseconds(1)).count());
  } while (std::chrono::steady_clock::now() < deadline);
  return std::unexpected("fixture urban preparation timed out");
}
}
#endif
