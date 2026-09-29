#include "Check.h"
#include "SourceSet.h"
#include "TerrainLoader.h"
#include "tiles/TerrainGrid.h"
#include <chrono>
#include <memory>
#include <optional>
#include <thread>

namespace {
using namespace outshine::Data;

class NoNetwork final : public Transport {
public:
  FetchStart Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class WholeSource final : public Source {
public:
  SourceDecl Decl{
      .Id = "non-tile-provider", .Revision = "owned-revision", .Keeps = Cacheability::Never};

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &) const noexcept override { return Address::Whole(73); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Delivered({1, 2, 3});
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const Data::TileId requested{.Zoom = 4, .X = 8, .Y = 8};
  for (const bool workerField : {false, true}) {
    std::optional<Data::FetchFailure> failure;
    std::string expectedKey;
    {
      Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
      Data::SourceSet sources(store);
      NoNetwork transport;
      auto source = std::make_unique<WholeSource>();
      expectedKey = Data::SourceKey(source->Decl);
      CHECK(sources.Add(std::move(source)) == Data::SourceSet::Registration::Accepted,
            "non-tile source registers without network access");
      TilePool pool({.Threads = 1, .PollAttempts = 10}, sources, transport);
      GroundStream ground(pool, {.Z = 4, .Grid = 4});
      bool refused = false;
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      do {
        if (workerField) {
          std::shared_ptr<const TerrainField> field;
          refused = pool.Field(requested, &field, &failure) == TilePool::Reply::Refused;
          CHECK(!field, "an invalid served address never publishes a height field");
        } else {
          const auto grid = ground.FieldOf(requested);
          refused = grid.Where() == TerrainGrid::State::Refused;
          failure = grid.Failure();
          CHECK(!grid.TryField(), "query-side adapter never publishes non-tile bytes");
        }
        if (!refused) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
      } while (!refused && std::chrono::steady_clock::now() < deadline);
      CHECK(refused, "both terrain adapters reach an explicit terminal refusal");
      if (!workerField) {
        auto block = GroundBlock::Waiting();
        const auto blockDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
          block = ground.BlockAt({.Zoom = 4, .X = 8, .Y = 8});
          if (block.Where() != GroundBlock::State::Pending) { break; }
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < blockDeadline);
        CHECK(block.Where() == GroundBlock::State::Missing,
              "a refused field remains missing when queried as a generator block");
      }
    }
    CHECK(failure && failure->Kind == Data::DataKind::Elevation &&
              failure->Reason == Data::FetchFailureReason::CorruptPayload &&
              failure->Requested == Data::Address::At(requested) &&
              failure->Served == Data::Address::Whole(73) &&
              failure->SourceId == "non-tile-provider" &&
              failure->SourceRevision == "owned-revision" && failure->SourceKey == expectedKey,
          "refusal owns actual requested/served/source context beyond adapter lifetime");
  }
  return Report();
}
