#include "Check.h"
#include "SourcedTerrainFields.h"

#include <array>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Error = SourcedTerrainFields::CaptureError;
  const Data::TileId parent{.Zoom = 1, .X = 0, .Y = 0};
  const Data::TileId exact{.Zoom = 2, .X = 0, .Y = 0};
  auto coarse = std::make_shared<Ground::TerrainField>(3, 3);
  auto fine = std::make_shared<Ground::TerrainField>(3, 3);
  auto unrelated = std::make_shared<Ground::TerrainField>(3, 3);
  for (const auto &field : {coarse, fine, unrelated}) {
    field->AddSource(
        {.Kind = Data::DataKind::Elevation, .Tile = parent, .SourceId = "dem", .Revision = "r1"});
    for (uint32_t y = 0; y < 3; ++y) {
      for (uint32_t x = 0; x < 3; ++x) { field->SetM(y, x, field == fine ? 90.0f : 30.0f); }
    }
  }
  const std::weak_ptr<const Ground::TerrainField> unused = unrelated;
  std::vector<SourcedTerrainFields::Entry> fields{
      {parent, coarse}, {exact, fine}, {{.Zoom = 1, .X = 1, .Y = 1}, unrelated}};
  const std::array<Ground::TileSpot, 3> requests{
      {{.Zoom = 2, .X = 0, .Y = 0}, {.Zoom = 2, .X = 1, .Y = 0}, {.Zoom = 2, .X = 1, .Y = 1}}};
  const size_t budget = requests.size() * sizeof(SourcedTerrainFields::Entry) +
                        coarse->HeapBytes() + fine->HeapBytes();
  auto captured = SourcedTerrainFields::Capture(fields, requests, budget);
  CHECK(captured && captured->RetainedBytes() == budget,
        "two child requests share one ancestor reservation; exact source remains distinct");
  const auto refused = SourcedTerrainFields::Capture(
      fields, requests, requests.size() * sizeof(SourcedTerrainFields::Entry) - 1);
  CHECK(!refused && refused.error() == Error::OverBudget,
        "one byte below the required pointer storage refuses the capture");
  const std::array<Ground::TileSpot, 1> missing{{{.Zoom = 1, .X = 1, .Y = 0}}};
  const auto absent = SourcedTerrainFields::Capture(fields, missing, budget);
  CHECK(!absent && absent.error() == Error::MissingSource,
        "uncovered requests cannot borrow unrelated terrain");
  for (const int zoom : {-1, 32}) {
    const std::array<Ground::TileSpot, 1> invalid{{{.Zoom = zoom, .X = 0, .Y = 0}}};
    const auto rejected = SourcedTerrainFields::Capture(fields, invalid, budget);
    CHECK(!rejected && rejected.error() == Error::InvalidRequest, "invalid zoom refuses capture");
  }
  fields.clear();
  coarse.reset();
  fine.reset();
  unrelated.reset();
  CHECK(unused.expired(), "unrequested raster is not retained by the captured subset");
  if (captured) {
    Ground::HeightField::Block a, b;
    CHECK(captured->CopySourcedField(exact, a) && a.Nodes.front() == 90.0f,
          "exact source remains alive after original owners release it");
    CHECK(captured->CopySourcedField({.Zoom = 2, .X = 1, .Y = 0}, b) && b.Nodes.front() == 30.0f &&
              b.At.Zoom == 2 && b.At.X == 1,
          "ancestor sampling preserves requested coordinates and source heights");
  }
  return Report();
}
