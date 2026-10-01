#include "OsmSourceLoaderState.h"

#include "SourceProviderValidation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine {
namespace {
bool Before(Data::GeoCellId left, Data::GeoCellId right) noexcept {
  return std::tie(left.Level, left.X, left.Y) < std::tie(right.Level, right.X, right.Y);
}

bool ValidCell(Data::GeoCellId cell) noexcept {
  const auto bounds = cell.Bounds();
  return bounds && (bounds->EastDeg - bounds->WestDeg) * (bounds->NorthDeg - bounds->SouthDeg) <=
                       Data::kOsmApiMaximumAreaDeg2;
}
}

size_t OsmSourceLoader::Cells::ChargedBytes() const noexcept {
  size_t bytes = 0;
  for (const auto &entry : Tracked) {
    if (!entry.Snapshot.expired()) { bytes += entry.ChargedBytes; }
  }
  return bytes;
}

std::vector<Data::GeoCellId> OsmSourceLoader::Cells::NextAcquisitionBatch() const {
  std::vector<Data::GeoCellId> batch;
  batch.reserve(3);
  const auto acquisitionOrder = [](Data::GeoCellId left, Data::GeoCellId right) {
    return left.Level != right.Level ? left.Level > right.Level : Before(left, right);
  };
  for (size_t at = 0; at < Wanted.size(); ++at) {
    if (Preparing[at].Snapshot) { continue; }
    const auto position = std::ranges::lower_bound(batch, Wanted[at], acquisitionOrder);
    if (position - batch.begin() == 2) { continue; }
    batch.insert(position, Wanted[at]);
    if (batch.size() > 2) { batch.pop_back(); }
  }
  return batch;
}

bool OsmSourceLoader::Cells::Ready() const noexcept {
  return std::ranges::all_of(Preparing,
                             [](const auto &entry) { return entry.Snapshot != nullptr; });
}

std::vector<OsmSourceLoader::CellSource> OsmSourceLoader::Cells::Reuse(
    std::span<const Data::GeoCellId> wanted, bool published, bool pending) const {
  std::vector<CellSource> preparing(wanted.size());
  for (size_t at = 0; at < wanted.size(); ++at) {
    if (published) {
      const auto found = std::ranges::lower_bound(
          Published, wanted[at], Before, [](const auto &entry) { return *entry.Snapshot->Cell; });
      if (found != Published.end() && found->Snapshot->Cell == wanted[at]) {
        preparing[at] = *found;
      }
    }
    if (pending && !preparing[at].Snapshot) {
      const auto found = std::ranges::lower_bound(Wanted, wanted[at], Before);
      const auto index = static_cast<size_t>(found - Wanted.begin());
      if (found != Wanted.end() && *found == wanted[at] && index < Preparing.size()) {
        preparing[at] = Preparing[index];
      }
    }
  }
  return preparing;
}

std::expected<void, std::string> OsmSourceLoader::Cells::Stage(std::vector<CellSource> ready) {
  std::erase_if(Tracked, [](const auto &entry) { return entry.Snapshot.expired(); });
  size_t bytes = ChargedBytes();
  if (bytes > Limits.SnapshotBytesMost || ready.size() > Limits.CellsMost * 2 - Tracked.size()) {
    return std::unexpected("original OSM cell snapshots exceed retained capacity");
  }
  std::vector<size_t> positions;
  positions.reserve(ready.size());
  for (const auto &entry : ready) {
    if (!entry.Snapshot) {
      return std::unexpected("original OSM cell result does not belong to current demand");
    }
    const auto cell = entry.Snapshot->Cell;
    if (!cell) {
      return std::unexpected("original OSM cell result does not belong to current demand");
    }
    const auto at = std::ranges::find(Wanted, *cell);
    if (at == Wanted.end()) {
      return std::unexpected("original OSM cell result does not belong to current demand");
    }
    positions.push_back(static_cast<size_t>(at - Wanted.begin()));
    if (entry.ChargedBytes > Limits.SnapshotBytesMost - bytes) {
      return std::unexpected("original OSM cell snapshots exceed the byte budget");
    }
    bytes += entry.ChargedBytes;
  }
  for (size_t index = 0; index < ready.size(); ++index) {
    auto &entry = ready[index];
    Tracked.push_back({.Snapshot = entry.Snapshot, .ChargedBytes = entry.ChargedBytes});
    Preparing[positions[index]] = std::move(entry);
  }
  return {};
}

std::span<const OsmSourceLoader::CellSource> OsmSourceLoader::CurrentCells() const noexcept {
  return Scope_ == Scope::Cells && Cells_ ? std::span(Cells_->Published)
                                          : std::span<const CellSource>{};
}

size_t OsmSourceLoader::CellSnapshotChargeBytes() const noexcept {
  return Cells_ ? Cells_->ChargedBytes() : 0;
}

std::expected<void, std::string>
OsmSourceLoader::RequestCells(const Data::SourceProvider &provider,
                              std::span<const Data::GeoCellId> cells,
                              CellLimits limits,
                              std::string_view root,
                              const Data::ProviderRegistry *registry) {
  if (auto valid = Data::ValidateSourceProviders(std::span(&provider, 1)); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  if (provider.Kind != "osm" || provider.Coverage || !provider.Location.empty() ||
      limits.CellsMost == 0 || limits.CellsMost > std::numeric_limits<size_t>::max() / 2 ||
      limits.SnapshotBytesMost == 0 || limits.LevelMost < 9 ||
      limits.LevelMost > Data::GeoCellId::MaximumLevel || cells.size() > limits.CellsMost ||
      std::ranges::any_of(cells, [limits](auto cell) { return cell.Level > limits.LevelMost; }) ||
      !std::ranges::all_of(cells, ValidCell)) {
    return std::unexpected("original OSM cell demand requires a catalogue and bounded valid cells");
  }
  std::vector<Data::GeoCellId> wanted(cells.begin(), cells.end());
  std::ranges::sort(wanted, Before);
  if (std::ranges::adjacent_find(wanted) != wanted.end()) {
    return std::unexpected("original OSM cell demand contains duplicate addresses");
  }
  if (Cells_ && (Cells_->ChargedBytes() > limits.SnapshotBytesMost ||
                 static_cast<size_t>(std::ranges::count_if(Cells_->Tracked, [](const auto &entry) {
                   return !entry.Snapshot.expired();
                 })) > limits.CellsMost * 2)) {
    return std::unexpected("original OSM cell demand cannot reduce limits below pinned snapshots");
  }
  for (auto cell : wanted) {
    while (cell.Level > 0) {
      --cell.Level;
      cell.X /= 2;
      cell.Y /= 2;
      if (std::ranges::binary_search(wanted, cell, Before)) {
        return std::unexpected("original OSM cell demand contains overlapping ancestors");
      }
    }
  }
  if (Scope_ == Scope::Cells && Requested_.front() == provider && Root_ == root &&
      Access_->Registry == registry && Cells_->Roots == wanted && Cells_->Limits == limits &&
      Phase_ != Phase::Failed) {
    return {};
  }
  if (Revision_ == std::numeric_limits<uint64_t>::max()) {
    return std::unexpected("original OSM source revision is exhausted");
  }
  if (!Cells_) { Cells_ = std::make_unique<Cells>(); }
  const bool published = Cells_->PublishedProvider == provider && Cells_->PublishedRoot == root &&
                         Cells_->PublishedRegistry == registry;
  const bool pending = Scope_ == Scope::Cells && Requested_.front() == provider && Root_ == root &&
                       Access_->Registry == registry;
  auto leaves = Cells_->SelectLeaves(wanted, published, pending);
  if (leaves.size() > limits.CellsMost ||
      std::ranges::any_of(leaves, [limits](auto cell) { return cell.Level > limits.LevelMost; })) {
    return std::unexpected("original OSM resident leaves exceed requested cell limits");
  }
  auto preparing = Cells_->Reuse(leaves, published, pending);
  ++Revision_;
  Scope_ = Scope::Cells;
  Requested_.assign(1, provider);
  Root_ = root;
  Access_->Registry = registry;
  Cells_->Limits = limits;
  Cells_->Roots = std::move(wanted);
  Cells_->Wanted = std::move(leaves);
  Cells_->Preparing = std::move(preparing);
  Error_.clear();
  if (Pending_) { (void)Pending_->Stop.request_stop(); }
  Phase_ = Phase::Loading;
  if (Cells_->Ready()) { CompleteCells({}); }
  Poll();
  return {};
}

void OsmSourceLoader::CompleteCells(std::vector<CellSource> ready) {
  if (auto staged = Cells_->Stage(std::move(ready)); !staged) {
    Error_ = std::move(staged.error());
    Cells_->Preparing.clear();
    Phase_ = Phase::Failed;
    return;
  }
  if (!Cells_->Ready()) { return; }
  Cells_->Published = std::move(Cells_->Preparing);
  Cells_->PublishedRoots = Cells_->Roots;
  Cells_->PublishedProvider = Requested_.front();
  Cells_->PublishedRoot = Root_;
  Cells_->PublishedRegistry = Access_->Registry;
  Current_.reset();
  PublishedRevision_ = Revision_;
  Phase_ = Cells_->Wanted.empty() ? Phase::Inactive : Phase::Ready;
}

}
