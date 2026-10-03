#include "OsmStructureCell.h"
#include "OsmSourceCapture.h"
#include "TangentFrame.h"
#include "Sha256.h"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <queue>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
std::expected<StructureDescription, std::string>
PrepareStructureCell(const std::shared_ptr<const SourceSnapshot> &source,
                     StructurePolicy policy,
                     const std::stop_token &stop) {
  if (!source || source->Coverage.empty()) {
    return std::unexpected("original buildings require declared source coverage");
  }
  if (stop.stop_requested()) { return std::unexpected("original building preparation canceled"); }
  auto buildings = BuildingFootprints::Build(source, policy.PointsMost);
  if (!buildings) {
    return std::unexpected("original building footprint failed at object " +
                           std::to_string(buildings.error().Source.Id) + " with code " +
                           std::to_string(static_cast<int>(buildings.error().Code)));
  }
  Data::SourceCoverage bounds = source->Coverage.front();
  for (const auto &coverage : source->Coverage) {
    bounds.WestDeg = std::min(bounds.WestDeg, coverage.WestDeg);
    bounds.SouthDeg = std::min(bounds.SouthDeg, coverage.SouthDeg);
    bounds.EastDeg = std::max(bounds.EastDeg, coverage.EastDeg);
    bounds.NorthDeg = std::max(bounds.NorthDeg, coverage.NorthDeg);
  }
  const auto points = buildings->Points();
  for (size_t point = 0; point < points.size(); point += 2u) {
    const auto at =
        TangentFrame::At({.LongitudeDeg = points[point + 1u], .LatitudeDeg = points[point]});
    const double half = policy.PointWidthM / 2.0;
    const auto low = at.ApproximateGeographicAt({.EastM = -half, .NorthM = -half});
    const auto high = at.ApproximateGeographicAt({.EastM = half, .NorthM = half});
    bounds.WestDeg = std::min(bounds.WestDeg, low.LongitudeDeg);
    bounds.SouthDeg = std::min(bounds.SouthDeg, low.LatitudeDeg);
    bounds.EastDeg = std::max(bounds.EastDeg, high.LongitudeDeg);
    bounds.NorthDeg = std::max(bounds.NorthDeg, high.LatitudeDeg);
  }
  if (stop.stop_requested()) { return std::unexpected("original building preparation canceled"); }
  auto described = DescribeStructures(*buildings, source, {.Bounds = bounds}, policy);
  if (!described) {
    return std::unexpected("original building input failed with code " +
                           std::to_string(static_cast<int>(described.error())));
  }
  return std::move(*described);
}

namespace {

void Append(std::string &bytes, uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) {
    bytes.push_back(static_cast<char>(value >> shift));
  }
}

void Append(std::string &bytes, std::string_view text) {
  Append(bytes, text.size());
  bytes.append(text);
}

void AppendTags(std::string &bytes, std::span<const Tag> tags) {
  Append(bytes, tags.size());
  for (const auto &tag : tags) {
    Append(bytes, tag.Key);
    Append(bytes, tag.Value);
  }
}

void Append(std::string &bytes, const Node &node) {
  Append(bytes, node.LatitudeDeg == 0 ? 0 : std::bit_cast<uint64_t>(node.LatitudeDeg));
  Append(bytes, node.LongitudeDeg == 0 ? 0 : std::bit_cast<uint64_t>(node.LongitudeDeg));
  AppendTags(bytes, node.Tags);
}

void Append(std::string &bytes, const Way &way) {
  Append(bytes, way.NodeIds.size());
  for (const uint64_t node : way.NodeIds) { Append(bytes, node); }
  AppendTags(bytes, way.Tags);
}

void Append(std::string &bytes, const Relation &relation) {
  Append(bytes, relation.Members.size());
  for (const auto &member : relation.Members) {
    Append(bytes, static_cast<uint64_t>(member.Kind));
    Append(bytes, member.Id);
    Append(bytes, member.Role);
  }
  AppendTags(bytes, relation.Tags);
}

template <typename Element>
std::expected<std::vector<StructureCell::ElementProof>, std::string>
ProveElements(std::span<const Element> elements, const std::stop_token &stop) {
  std::vector<StructureCell::ElementProof> proofs;
  proofs.reserve(elements.size());
  std::string bytes;
  for (const auto &element : elements) {
    if (stop.stop_requested()) { return std::unexpected("original cell compilation canceled"); }
    bytes.clear();
    Append(bytes, element);
    const StructureCell::ElementProof proof{.Id = element.Id,
                                            .Digest = Sha256Digest(bytes.data(), bytes.size())};
    proofs.push_back(proof);
  }
  return proofs;
}

class StructureCellCompiler final : public CellCompiler {
public:
  explicit StructureCellCompiler(StructurePolicy policy) : Policy_(policy) {}

  [[nodiscard]] std::expected<std::shared_ptr<const CellProduct>, std::string>
  Compile(std::shared_ptr<const SourceSnapshot> source,
          const std::stop_token &stop) const override {
    auto description = PrepareStructureCell(source, Policy_, stop);
    if (!description) { return std::unexpected(std::move(description.error())); }
    auto result = std::make_shared<StructureCell>();
    result->Description = std::move(*description);
    auto nodes = ProveElements(source->Elements.Nodes(), stop);
    if (!nodes) { return std::unexpected(std::move(nodes.error())); }
    auto ways = ProveElements(source->Elements.Ways(), stop);
    if (!ways) { return std::unexpected(std::move(ways.error())); }
    auto relations = ProveElements(source->Elements.Relations(), stop);
    if (!relations) { return std::unexpected(std::move(relations.error())); }
    result->Elements = {std::move(*nodes), std::move(*ways), std::move(*relations)};
    return result;
  }

private:
  StructurePolicy Policy_;
};

std::expected<void, std::string>
VerifyElements(std::span<const std::shared_ptr<const StructureCell>> cells,
               size_t kind,
               const std::stop_token &stop) {
  struct Cursor {
    std::span<const StructureCell::ElementProof> Elements;
    size_t At = 0;

    [[nodiscard]] const StructureCell::ElementProof &Current() const { return Elements[At]; }
  };

  const auto later = [](const Cursor &a, const Cursor &b) {
    return a.Current().Id > b.Current().Id;
  };
  std::vector<Cursor> cursors;
  cursors.reserve(cells.size());
  for (const auto &cell : cells) {
    if (!cell->Elements[kind].empty()) { cursors.push_back({.Elements = cell->Elements[kind]}); }
  }
  std::priority_queue<Cursor, std::vector<Cursor>, decltype(later)> pending(later,
                                                                            std::move(cursors));
  const StructureCell::ElementProof *last = nullptr;
  while (!pending.empty()) {
    if (stop.stop_requested()) { return std::unexpected("original cell verification canceled"); }
    auto next = pending.top();
    pending.pop();
    const auto &element = next.Current();
    if (last != nullptr && last->Id == element.Id && last->Digest != element.Digest) {
      return std::unexpected("original cells disagree at object " + std::to_string(element.Id));
    }
    last = &element;
    if (++next.At < next.Elements.size()) { pending.push(next); }
  }
  return {};
}

}

size_t StructureCell::StorageChargeBytes() const noexcept {
  const auto &prints = Description.Footprints;
  size_t bytes =
      sizeof(StructureCell) + prints.LatLon.capacity() * sizeof(double) +
      prints.Rings.capacity() * sizeof(GeographicRing) +
      prints.Structures.capacity() * sizeof(outshine::Ground::StructureFootprints::Structure);
  for (const auto &kind : Elements) { bytes += kind.capacity() * sizeof(ElementProof); }
  assert(Description.Source);
  bytes += sizeof(SourceCapture) +
           static_cast<const SourceCapture &>(*Description.Source).Snapshot().StorageChargeBytes();
  const auto &provenance = *prints.Origin.Provenance;
  bytes += sizeof(Data::SourceProvenance) + provenance.DatasetId.capacity() +
           provenance.Revision.capacity() + 2 +
           provenance.PayloadSha256.capacity() * sizeof(std::string);
  for (const auto &digest : provenance.PayloadSha256) { bytes += digest.capacity() + 1; }
  return bytes;
}

std::shared_ptr<const CellCompiler> MakeStructureCellCompiler(StructurePolicy policy) {
  return std::make_shared<const StructureCellCompiler>(policy);
}

std::expected<void, std::string>
VerifyStructureCells(std::span<const std::shared_ptr<const StructureCell>> cells,
                     const std::stop_token &stop) {
  for (const auto &cell : cells) {
    if (!cell || !cell->Description.Footprints.Origin.Provenance ||
        cell->Description.Footprints.Origin.Provenance->DatasetId !=
            cells.front()->Description.Footprints.Origin.Provenance->DatasetId ||
        cell->Description.Footprints.Origin.Provenance->Revision !=
            cells.front()->Description.Footprints.Origin.Provenance->Revision) {
      return std::unexpected("original building cells require one dataset revision");
    }
  }
  for (size_t kind = 0; kind < 3; ++kind) {
    if (auto checked = VerifyElements(cells, kind, stop); !checked) { return checked; }
  }
  return {};
}

}
