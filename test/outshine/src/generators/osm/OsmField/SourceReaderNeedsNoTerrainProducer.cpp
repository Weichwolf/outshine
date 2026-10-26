#include "Check.h"
#include "MvtWireFixture.h"
#include "OsmField.h"
#include "Sha256.h"
#include "SourceReader.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;

class ControlledReader final : public Data::SourceReader {
public:
  Data::SourceDelivery::State State = Data::SourceDelivery::State::Pending;
  Data::Address Last = Data::Address::Whole(0);
  Data::DataKind Kind = Data::DataKind::Elevation;
  size_t Polls = 0;
  std::vector<uint8_t> Payload = Test::Mvt::BuildingTile();

  [[nodiscard]] Data::SourceDelivery Poll(const Data::SourceRequest &request) override {
    ++Polls;
    Last = request.Where();
    Kind = request.Kind();
    switch (State) {
      case Data::SourceDelivery::State::Pending: return Data::SourceDelivery::Waiting();
      case Data::SourceDelivery::State::Vacant: return Data::SourceDelivery::Nothing();
      case Data::SourceDelivery::State::Undeclared: return Data::SourceDelivery::NoSource();
      case Data::SourceDelivery::State::Refused: return Data::SourceDelivery::Wire();
      case Data::SourceDelivery::State::Consumed: return Data::SourceDelivery::Consumed();
      case Data::SourceDelivery::State::Delivered:
        return Data::SourceDelivery::From("fixture.vector", "r1", Last, Payload, "binding");
    }
    std::unreachable();
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::Generators::Osm;
  const std::array<std::string, 1> layers{"building"};
  constexpr Data::TileId address{.Zoom = 2, .X = 1, .Y = 1};
  ControlledReader reader;
  OsmField field(address.Zoom, layers, MvtSchema::OpenMapTiles);
  const auto waiting = field.PollTile(reader, {.X = 1, .Y = 1});
  CHECK(waiting && !*waiting && field.Features().empty() && reader.Polls == 1,
        "pending input produces no geometry and needs no terrain source");
  reader.State = Data::SourceDelivery::State::Delivered;
  const auto ready = field.PollTile(reader, {.X = 1, .Y = 1});
  CHECK(ready && *ready && field.Features().size() == 1 && reader.Polls == 2 &&
            reader.Kind == Data::DataKind::VectorMap && reader.Last.Tile() == address,
        "a source delivery alone produces a real normalized OSM building");
  if (field.Tiles().size() == 1) {
    const auto &tile = field.Tiles().front();
    CHECK(tile.Source.SourceId == "fixture.vector" && tile.Source.Revision == "r1" &&
              tile.InputDigest == Sha256Hex(reader.Payload.data(), reader.Payload.size()),
          "source identity and exact input digest survive normalization");
  } else {
    CHECK(false, "one normalized tile records its source identity");
  }
  CHECK(field.PollTile(reader, {.X = 1, .Y = 1}) && reader.Polls == 2,
        "settled input is not requested again");
  for (const auto state :
       {Data::SourceDelivery::State::Vacant, Data::SourceDelivery::State::Undeclared}) {
    reader.State = state;
    OsmField absent(address.Zoom, layers, MvtSchema::OpenMapTiles);
    const auto settled = absent.PollTile(reader, {.X = 1, .Y = 1});
    CHECK(settled && *settled && absent.Features().empty(),
          "absent and undeclared inputs settle without invented geometry");
  }
  for (const auto state :
       {Data::SourceDelivery::State::Refused, Data::SourceDelivery::State::Consumed}) {
    reader.State = state;
    OsmField refused(address.Zoom, layers, MvtSchema::OpenMapTiles);
    CHECK(!refused.PollTile(reader, {.X = 1, .Y = 1}) && refused.Features().empty(),
          "refused or invalid delivery never publishes a partial tile");
  }
  reader.State = Data::SourceDelivery::State::Delivered;
  constexpr uint8_t kInvalidTag = 0xff;
  reader.Payload = {kInvalidTag};
  OsmField corrupt(address.Zoom, layers, MvtSchema::OpenMapTiles);
  CHECK(!corrupt.PollTile(reader, {.X = 1, .Y = 1}) && corrupt.Features().empty(),
        "corrupt bytes remain a normalization error without terrain fallback");
  return Report();
}
