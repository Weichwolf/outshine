#include "TilePool.h"
#include "TerrainLoader.h"
#include "tiles/TerrainTiles.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "Transport.h"
#include "Check.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Ground;
constexpr uint8_t kPng[]{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x08, 0x02, 0x00, 0x00, 0x00, 0x4b,
    0x6d, 0x29, 0xdc, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x68,
    0x60, 0x60, 0xc0, 0x8a, 0xb0, 0x8b, 0x0e, 0x5a, 0x09, 0x00, 0xa1, 0x7c, 0x20, 0x01, 0x64,
    0xc6, 0x93, 0x18, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

class NoNetwork final : public Transport {
public:
  FetchStart Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class Provider final : public Source {
public:
  SourceDecl Decl{.Id = "shape-transition-dem", .Revision = "r1", .Keeps = Cacheability::Never};

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Delivered({kPng, kPng + sizeof(kPng)});
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  for (const bool querySide : {false, true}) {
    Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
    Data::SourceSet sources(store);
    NoNetwork transport;
    CHECK(sources.Add(std::make_unique<Provider>()) == Data::SourceSet::Registration::Accepted,
          "independent provider registers");
    TilePool pool(
        {.Threads = 1, .ByteBudget = 1024u * 1024u, .PollAttempts = 10}, sources, transport);
    GroundStream ground(pool, {.Z = 2, .Grid = 4});
    const Data::TileId at{.Zoom = 2, .X = 1, .Y = 1};
    std::shared_ptr<const TerrainField> previous;
    double previousAmplitude = 0.0;
    for (const double amplitude : {0.0, 5.0, 9.0, 0.0}) {
      const bool shaped = amplitude != 0.0;
      pool.Shapes(
          shaped ? ShapedGround{.Kind = "sineRidge", .AmplitudeM = amplitude, .WavelengthM = 1.0e12}
                 : ShapedGround{});
      if (previous && previous->Certificate().IsComplete()) {
        CHECK(!pool.CertificateCurrent(previous->Certificate()),
              "shape transition revokes old provider certificate");
      }
      const uint64_t revision = pool.TerrainScopeRevision();
      const auto declaration = pool.Shaped();
      pool.Shapes(declaration);
      CHECK(pool.TerrainScopeRevision() == revision,
            "an identical declaration does not revoke terrain scope");
      std::shared_ptr<const TerrainField> field;
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      do {
        const auto result =
            querySide ? ground.PollStitchedField(at, field) : pool.Field(at, &field);
        if (result == TilePool::Reply::Ready) { break; }
        (void)pool.AwaitLanding(0.01);
      } while (std::chrono::steady_clock::now() < deadline);
      CHECK(field != nullptr, "each terrain declaration yields a worker product");
      if (field) {
        CHECK(field->Certificate().TerrainScopeRevision() == revision &&
                  field->Certificate().ScopeCurrent(revision),
              "provider and shaped snapshots both retain their captured terrain scope");
      }
      if (field && !shaped) {
        CHECK(field->Certificate().TerrainScopeRevision() == revision &&
                  pool.CertificateCurrent(field->Certificate()),
              "provider certificate captures current pool scope");
      }
      if (!field) { continue; }
      if (previous) {
        CHECK(std::abs(static_cast<double>(previous->AtM(3, 3)) - previousAmplitude) < 1.0e-4,
              "a borrowed old field remains immutable through shape transitions");
      }
      previous = field;
      previousAmplitude = amplitude;
      if (querySide) {
        CHECK(ground.ResidentStitchedField(at) == field,
              "unchanged scope retains the current stitched product");
        const LongitudeLatitude eye{.LongitudeDeg = -45, .LatitudeDeg = 45};
        const auto sample = ground.At(eye).AslM();
        CHECK(sample && std::abs(*sample - amplitude) < 1.0e-4,
              "sampled ground follows the same terrain transition");
      }
      CHECK(std::abs(static_cast<double>(field->AtM(3, 3)) - amplitude) < 1.0e-4,
            "worker output follows the current analytical terrain declaration");
      CHECK(!field->Sources().empty() &&
                std::ranges::all_of(field->Sources(),
                                    [shaped](const auto &source) {
                                      return (source.From ==
                                              Data::TileSourceIdentity::Origin::Shaped) == shaped;
                                    }),
            "returning to empty Kind restores provider provenance");
    }
    TileBuild mesh;
    const auto awaitMesh = [&] {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      do {
        if (pool.Mesh(at, 4, &mesh) == TilePool::Reply::Ready) { return true; }
        (void)pool.AwaitLanding(0.01);
      } while (std::chrono::steady_clock::now() < deadline);
      return false;
    };
    pool.Shapes({});
    CHECK(awaitMesh(), "provider mesh becomes resident");
    pool.Shapes({.Kind = "sineRidge", .AmplitudeM = 9.0, .WavelengthM = 1.0e12});
    CHECK(pool.Wants(at, 4) != TilePool::Reply::Ready,
          "a cached provider mesh cannot claim readiness for the new scope");
    CHECK(awaitMesh(), "new scope replaces a held mesh result");
    const auto side = static_cast<size_t>(mesh.Side);
    CHECK(side >= 2 && mesh.Nodes.size() == side * side &&
              std::abs(mesh.Nodes[(side / 2) * side + side / 2] - 9.0f) < 1.0e-4f,
          "mesh-cache replacement carries the new analytical heights");
  }
  return Report();
}
