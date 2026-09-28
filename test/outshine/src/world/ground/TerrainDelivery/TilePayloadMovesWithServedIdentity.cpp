#include "Check.h"
#include "TerrainDelivery.h"

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const Data::TileId requested{.Zoom = 3, .X = 4, .Y = 3};
  const Data::TileId ancestor{.Zoom = 2, .X = 2, .Y = 1};
  const Data::Fetch request(Data::DataKind::Elevation, Data::Address::At(requested));
  TilePool::Landing landing{.Bytes = {1, 2, 3},
                            .SourceId = "ancestor-provider",
                            .SourceRevision = "actual-revision",
                            .SourceKey = "actual-owned-source-key",
                            .At = Data::Address::At(ancestor)};
  const auto *allocation = landing.Bytes.data();
  auto delivered = FromTerrainDelivery(request, std::move(landing));
  CHECK(delivered.Where() == TerrainBytes::State::Delivered && !delivered.Failure(),
        "tile delivery retains the normal payload state");
  auto payload = delivered.Take();
  CHECK(payload && payload->Png.data() == allocation &&
            payload->Png == std::vector<uint8_t>({1, 2, 3}),
        "the adapter moves the existing byte allocation without copying it");
  CHECK(payload && payload->At == ancestor && payload->Source.Tile == ancestor &&
            payload->Source.Kind == Data::DataKind::Elevation &&
            payload->Source.SourceId == "ancestor-provider" &&
            payload->Source.Revision == "actual-revision" &&
            payload->SourceKey == "actual-owned-source-key",
        "fallback payload retains its served ancestor and declared source identity");
  CHECK(!delivered.Take(), "a delivered payload has exactly one owner");
  return Report();
}
