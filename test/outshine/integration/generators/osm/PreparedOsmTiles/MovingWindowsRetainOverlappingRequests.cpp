#include "PreparedOsmTiles.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include "OsmCacheFixture.h"
#include "TileGeodesy.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <expected>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr size_t kSide = 16;

struct Counts {
  std::atomic_uint Starts{0}, Cancels{0};
};

struct SourceState {
  std::array<Counts, kSide * kSide> Tiles;
  std::atomic_bool Released{false};
  std::atomic_uint64_t NextTicket{0};

  Counts &At(size_t x, size_t y) { return Tiles[y * kSide + x]; }
};

class DelayedSource final : public Source {
public:
  explicit DelayedSource(SourceState &state) : State_(state) {
    Decl_.Id = "moving-vector";
    Decl_.Revision = "r1";
    Decl_.Kind = DataKind::VectorMap;
    Decl_.Wire = WireFormat::MapboxVectorTile;
    Decl_.Keeps = Cacheability::Never;
    Decl_.MaximumPayloadBytes = Mvt::BuildingTile().size();
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage
  Covers([[maybe_unused]] const SourceRequest &request) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address &at, [[maybe_unused]] SourceTransport &transport) const override {
    const auto tile = at.Tile();
    if (!tile || tile->X >= kSide || tile->Y >= kSide) {
      return std::unexpected(SourceFailureReason::ProviderRefused);
    }
    const size_t index = tile->Y * kSide + tile->X;
    ++State_.Tiles[index].Starts;
    return static_cast<Ticket>(++State_.NextTicket * (kSide * kSide) + index + 1);
  }

  SourceResponse Collect([[maybe_unused]] const Address &at,
                         [[maybe_unused]] Ticket ticket,
                         [[maybe_unused]] SourceTransport &transport) const override {
    if (!State_.Released.load()) { return SourceResponse::Working(); }
    return SourceResponse::Delivered(Mvt::BuildingTile());
  }

  void Cancel(Ticket ticket, [[maybe_unused]] SourceTransport &transport) const override {
    ++State_.Tiles[(static_cast<size_t>(ticket) - 1) % (kSide * kSide)].Cancels;
  }

private:
  SourceState &State_;
  SourceDecl Decl_;
};

class FrameReader final : public SourceReader {
public:
  unsigned Calls = 0;

  SourceDelivery Poll([[maybe_unused]] const SourceRequest &request) override {
    ++Calls;
    return SourceDelivery::NoSource();
  }
};

struct Fixture {
  SourceState Source;
  OsmCache::NoTransport Transport;
  Tasks Compute{2};
  FrameReader Reader;
  std::shared_ptr<SourceSession> Session;
  std::shared_ptr<PreparedOsmTiles> Cache;
  std::unique_ptr<OsmField> Field;

  bool Open(const std::filesystem::path &directory) {
    auto session = OsmCache::OpenSession(std::make_unique<DelayedSource>(Source));
    CHECK(session, "a controllable source opens for the actual moving window");
    if (!session) { return false; }
    Session = std::move(*session);
    auto cache = PreparedOsmTiles::Open(directory.string(), Session, Transport, Compute);
    CHECK(cache, "the actual shared native pipeline opens for window transitions");
    if (!cache) { return false; }
    Cache = std::move(*cache);
    const std::array<std::string, 1> layers{"buildings"};
    Field = std::make_unique<OsmField>(4, layers, MvtSchema::OpenMapTiles, Cache);
    return true;
  }

  bool Pump(TileAt centre, int radius, OsmField *other = nullptr) {
    const auto geo = Ground::TileFracToGeo(
        {.X = static_cast<double>(centre.X) + 0.5, .Y = static_cast<double>(centre.Y) + 0.5}, 4);
    auto &field = other != nullptr ? *other : *Field;
    const auto result = field.Build(
        Reader, {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}, radius, 9);
    if (!result) { return false; }
    (void)Compute.AwaitCompletion(0.001);
    return true;
  }

  template <class Ready>
  bool Until(TileAt centre, int radius, Ready ready, OsmField *other = nullptr) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < until) {
      if (!Pump(centre, radius, other)) { return false; }
      if (ready()) { return true; }
    }
    return false;
  }
};

bool Started(SourceState &state, std::pair<size_t, size_t> columns) {
  const auto [firstX, lastX] = columns;
  for (size_t y = 4; y <= 6; ++y) {
    for (size_t x = firstX; x <= lastX; ++x) {
      if (state.At(x, y).Starts <= state.At(x, y).Cancels) { return false; }
    }
  }
  return true;
}

void RetainAndShrink(const std::filesystem::path &directory) {
  Fixture fixture;
  if (!fixture.Open(directory)) { return; }
  CHECK(fixture.Until({.X = 5, .Y = 5}, 1, [&] { return Started(fixture.Source, {4, 6}); }),
        "all nine original source reads are deliberately held pending");
  CHECK(fixture.Until({.X = 6, .Y = 5},
                      1,
                      [&] {
                        return Started(fixture.Source, {5, 7}) &&
                               fixture.Source.At(4, 4).Cancels == 1 &&
                               fixture.Source.At(4, 5).Cancels == 1 &&
                               fixture.Source.At(4, 6).Cancels == 1;
                      }),
        "moving the window starts its new column and withdraws only the old column");
  for (size_t y = 4; y <= 6; ++y) {
    for (size_t x = 5; x <= 6; ++x) {
      CHECK(fixture.Source.At(x, y).Starts == 1 && fixture.Source.At(x, y).Cancels == 0,
            "an overlapping in-flight source request survives motion without cancel or restart");
    }
  }
  CHECK(fixture.Until({.X = 6, .Y = 5},
                      0,
                      [&] {
                        for (size_t y = 4; y <= 6; ++y) {
                          for (size_t x = 5; x <= 7; ++x) {
                            if ((x != 6 || y != 5) &&
                                fixture.Source.At(x, y).Starts != fixture.Source.At(x, y).Cancels) {
                              return false;
                            }
                          }
                        }
                        return true;
                      }),
        "a radius change at the same centre releases pending tiles outside the new footprint");
  CHECK(fixture.Source.At(6, 5).Starts == 1 && fixture.Source.At(6, 5).Cancels == 0,
        "shrinking retains the exact original centre request");
  fixture.Source.Released = true;
  CHECK(fixture.Until({.X = 6, .Y = 5}, 0, [&] { return fixture.Field->SettledWithin(0); }),
        "the surviving request completes and publishes after motion and shrink");
  CHECK(fixture.Field->Tiles().size() == 1 && fixture.Cache->Costs().Generations == 1 &&
            fixture.Cache->Costs().Writes == 1 && fixture.Reader.Calls == 0,
        "only the needed tile is generated and no source IO occurs through the frame reader");
}

bool WaitCancellation(Fixture &fixture) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  bool cancelled = false;
  while (!cancelled && std::chrono::steady_clock::now() < until) {
    (void)fixture.Compute.AwaitCompletion(0.001);
    cancelled = true;
    for (size_t y = 4; y <= 6; ++y) {
      for (size_t x = 4; x <= 6; ++x) {
        cancelled = cancelled && fixture.Source.At(x, y).Cancels == 1;
      }
    }
  }
  return cancelled;
}

void WithdrawDestroyedOwner(const std::filesystem::path &directory) {
  Fixture fixture;
  if (!fixture.Open(directory)) { return; }
  CHECK(fixture.Until({.X = 5, .Y = 5}, 1, [&] { return Started(fixture.Source, {4, 6}); }),
        "pending requests exist before the owning field is destroyed");
  fixture.Field.reset();
  CHECK(WaitCancellation(fixture) && fixture.Reader.Calls == 0,
        "field destruction cancels its source reads without requiring another frame acquisition");
  CHECK(fixture.Cache->Costs().Generations == 0 && fixture.Cache->Costs().Writes == 0,
        "destroyed demand starts no generation or native publication");
}

void DeclaredReplacement(const std::filesystem::path &directory) {
  Fixture fixture;
  if (!fixture.Open(directory)) { return; }
  CHECK(fixture.Until({.X = 5, .Y = 5}, 1, [&] { return Started(fixture.Source, {4, 6}); }),
        "source work is pending before an explicit declared replacement");
  fixture.Field->Declare({}, {.X = 5, .Y = 5});
  CHECK(WaitCancellation(fixture) && fixture.Field->SettledWithin(0) &&
            fixture.Field->Features().empty() && fixture.Cache->Costs().Generations == 0,
        "declared content withdraws old source interest without destroying its live field");
}

void IndependentOwners(const std::filesystem::path &directory) {
  Fixture fixture;
  if (!fixture.Open(directory)) { return; }
  const std::array<std::string, 1> layers{"buildings"};
  auto second = std::make_unique<OsmField>(4, layers, MvtSchema::OpenMapTiles, fixture.Cache);
  CHECK(fixture.Until({.X = 5, .Y = 5},
                      1,
                      [&] {
                        return fixture.Pump({.X = 5, .Y = 5}, 1, second.get()) &&
                               Started(fixture.Source, {4, 6});
                      }),
        "two field owners share the same nine delayed native jobs");
  CHECK(fixture.Until({.X = 6, .Y = 5},
                      1,
                      [&] {
                        return fixture.Pump({.X = 5, .Y = 5}, 1, second.get()) &&
                               Started(fixture.Source, {4, 7});
                      }),
        "one moving owner adds its new column while the other keeps the original footprint");
  fixture.Field.reset();
  bool valid = true;
  for (int tick = 0; tick < 16; ++tick) {
    valid = fixture.Pump({.X = 5, .Y = 5}, 1, second.get()) && valid;
  }
  CHECK(valid, "the remaining owner keeps advancing after the other owner disappears");
  for (size_t y = 4; y <= 6; ++y) {
    for (size_t x = 4; x <= 6; ++x) {
      CHECK(fixture.Source.At(x, y).Starts == 1 && fixture.Source.At(x, y).Cancels == 0,
            "withdrawal of one owner preserves the other owner's exact in-flight source work");
    }
  }
  fixture.Source.Released = true;
  CHECK(fixture.Until(
            {.X = 5, .Y = 5}, 1, [&] { return second->SettledWithin(1); }, second.get()),
        "the surviving owner publishes the complete canonical window");
  CHECK(second->Tiles().size() == 9 && fixture.Cache->Costs().Generations == 9 &&
            fixture.Cache->Costs().Writes == 9 && fixture.Reader.Calls == 0,
        "shared work produces and publishes each required tile once");
}

void DirectionChanges(const std::filesystem::path &directory) {
  Fixture fixture;
  if (!fixture.Open(directory)) { return; }
  CHECK(fixture.Until({.X = 5, .Y = 5}, 1, [&] { return Started(fixture.Source, {4, 6}); }),
        "the direction-change test starts from a fully pending original window");
  bool valid = true;
  for (int tick = 0; tick < 64; ++tick) {
    valid = fixture.Pump({.X = tick % 2 == 0 ? 6 : 5, .Y = 5}, 1) && valid;
    const auto counts = fixture.Cache->AdmissionCounts();
    valid = valid && counts.Consumers <= 128 && counts.Tickets <= 128 &&
            counts.InputBytes <= size_t{512} * 1024 * 1024;
  }
  CHECK(
      valid,
      "rapid return to cancelled edge addresses stays pending instead of becoming a stale failure");
  for (size_t y = 4; y <= 6; ++y) {
    for (size_t x = 5; x <= 6; ++x) {
      CHECK(fixture.Source.At(x, y).Starts == 1 && fixture.Source.At(x, y).Cancels == 0,
            "the overlap remains uninterrupted across repeated direction changes");
    }
  }
  fixture.Source.Released = true;
  CHECK(fixture.Until({.X = 5, .Y = 5}, 1, [&] { return fixture.Field->SettledWithin(1); }),
        "the final window completes after repeated motion without a stuck cancellation group");
}

}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-moving-osm-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  RetainAndShrink(directory / "overlap");
  WithdrawDestroyedOwner(directory / "destroyed");
  DeclaredReplacement(directory / "declared");
  IndependentOwners(directory / "shared");
  DirectionChanges(directory / "turns");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated moving-world cache files retire after their owners");
  return Report();
}
