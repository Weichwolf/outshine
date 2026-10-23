#include "BuildingField.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

namespace {
using Field = outshine::Generators::Osm::BuildingField;
using Print = outshine::Ground::BuildingFootprint;

void Accept(Field &field, uint32_t tile, const Field::Baked &baked) {
  field.Take(tile);
  auto pending = field.PrepareAcceptance(tile, baked);
  field.CommitAcceptance(std::move(pending), baked);
}

Field::Baked Product(std::span<const Print> prints, std::span<const double> values) {
  return {.Coordinates = {}, .Prints = prints, .SeatSpreadM = values, .AcrossM = values};
}
}

int main() {
  using namespace outshine::Test;
  constexpr uint32_t kWest = 2;
  constexpr uint32_t kEmpty = 5;
  constexpr uint32_t kLocal = 7;
  constexpr uint32_t kEast = 9;
  constexpr uint32_t kPending = 99;
  const std::array<Print, 1> west{
      {{.HeightM = 20, .Street = {}, .TerrainCeiling = {}, .TerrainContacts = {}}}};
  const std::array<Print, 1> east{
      {{.HeightM = 90, .Street = {}, .TerrainCeiling = {}, .TerrainContacts = {}}}};
  const std::array<double, 1> westValues{2};
  const std::array<double, 1> eastValues{9};
  Field live;
  live.AnchorAt({{0, 0, 0}});
  Accept(live, kEast, Product(east, eastValues));
  Accept(live, kEmpty, Field::Baked{});
  Accept(live, kWest, Product(west, westValues));
  live.Take(kPending);
  const auto published = std::make_shared<const Field>(live.SnapshotAccepted());
  Field candidate = live.SnapshotAccepted();
  CHECK(candidate.IngestedTiles() == 3 && live.IngestedTiles() == 4 &&
            candidate.ReservationOwner() != live.ReservationOwner() &&
            candidate.ReservationOwner() != published->ReservationOwner(),
        "snapshots release only their unaccepted reservations and own independent domains");
  CHECK(candidate.OfTile(kWest).data() == live.OfTile(kWest).data() &&
            candidate.OfTile(kEast).data() == published->OfTile(kEast).data() &&
            &candidate.ProductAt(1) == &published->ProductAt(1),
        "accepted physical arrays and empty products are shared without footprint copies");
  CHECK(candidate.HeapBytes(Field::HeapScope::Additional) < candidate.HeapBytes(),
        "additional snapshot bytes exclude reachable shared physical products");
  candidate.Take(kLocal);
  const auto local = Product(east, eastValues);
  auto pending = candidate.PrepareAcceptance(kLocal, local);
  CHECK(candidate.OfTile(kLocal).empty() && live.OfTile(kLocal).empty(),
        "preparation alone cannot publish a local acceptance");
  candidate.CommitAcceptance(std::move(pending), local);
  const Field retained = candidate.SnapshotAccepted();
  CHECK(retained.OfTile(kLocal).size() == 1 && published->OfTile(kLocal).empty() &&
            live.OfTile(kLocal).empty() && candidate.FootprintCount() == 3,
        "a later snapshot retains local unpublished acceptances without changing prior worlds");
  CHECK(candidate.OfTile(kWest).data() == published->OfTile(kWest).data() &&
            candidate.OfTile(kEast).data() == published->OfTile(kEast).data(),
        "a new local tile leaves every unchanged physical product in place");
  const auto *westOwner = &candidate.ProductAt(0);
  const auto revision = candidate.Revision();
  auto same = Product(west, westValues);
  same.CellShellErrorM[0] = 0.5;
  auto refinement = candidate.PrepareAcceptance(kWest, same);
  candidate.ReplaceAcceptance(std::move(refinement), same);
  CHECK(&candidate.ProductAt(0) == westOwner && candidate.Revision() == revision &&
            candidate.InputOfTile(kWest)->CellShellErrorM[0] == 0.5 &&
            published->InputOfTile(kWest)->CellShellErrorM[0] == 0,
        "an unchanged physical hit reuses arrays while local selection metadata stays independent");
  const std::array<Print, 2> larger{{east.front(), west.front()}};
  const std::array<double, 2> largerValues{5, 6};
  const auto replacement = Product(larger, largerValues);
  auto replace = candidate.PrepareAcceptance(kEmpty, replacement);
  candidate.ReplaceAcceptance(std::move(replace), replacement);
  CHECK(candidate.FootprintCount() == 5 && candidate.OfTile(kEmpty).size() == 2 &&
            published->OfTile(kEmpty).empty() && retained.OfTile(kEmpty).empty() &&
            candidate.OfTile(kLocal).data() == retained.OfTile(kLocal).data(),
        "local replacement cannot lose another local tile or alter older snapshots");
  const auto pinned = candidate.PinProducts();
  CHECK(pinned.size() == 4 && pinned[0].FirstFootprint == 0 && pinned[1].FirstFootprint == 1 &&
            pinned[2].FirstFootprint == 3 && pinned[3].FirstFootprint == 4,
        "global footprint offsets follow accepted tile order across larger replacements");
  candidate.ResetDerived();
  live.Release(kPending);
  live.ResetDerived();
  CHECK(published->FootprintCount() == 2 && retained.FootprintCount() == 3 &&
            pinned[1].Physical->Prints.size() == 2 &&
            pinned[3].Physical->Prints.front().HeightM == 90,
        "snapshots and explicit pins own their products after producers reset");
  return Report();
}
