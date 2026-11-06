#include "RoadPageAsset.h"
#include "BinaryValueArchive.h"
#include "ClusterCook.h"
#include "ClusterValueFields.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <variant>

namespace outshine::Generators {
namespace {
constexpr uint64_t kLegacyFormat = 0x000231504452ULL;
constexpr uint64_t kAppearanceFormat = 0x000331504452ULL;
constexpr uint64_t kDisplayFormat = 0x000431504452ULL;
constexpr uint64_t kOriginalBodiesFormat = 0x000531504452ULL;
constexpr uint64_t kClusteredFormat = 0x000631504452ULL;
constexpr uint64_t kStoredSurfaceFormat = 0x000731504452ULL;
constexpr size_t kClusterBytes = 10 * sizeof(float) + 3 * sizeof(uint32_t);

bool HashKey(const std::string &key) {
  return key.size() == 64 && std::ranges::all_of(key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}

bool Before(const RoadPagePart &left, const RoadPagePart &right) {
  return left.Kind != right.Kind ? left.Kind < right.Kind : left.Index < right.Index;
}

bool ValidAddress(const RoadPageAddress &address) {
  if ((address.DisplayPolicy != RoadDisplayPolicy::UpperOnly &&
       address.DisplayPolicy != RoadDisplayPolicy::UpperWithOriginalBodies &&
       address.DisplayPolicy != RoadDisplayPolicy::StoredSurfaceEnvelope) ||
      (!address.PositionErrorM && address.DisplayPolicy != RoadDisplayPolicy::UpperOnly)) {
    return false;
  }
  if (address.PositionErrorM &&
      (!std::isfinite(*address.PositionErrorM) || *address.PositionErrorM <= 0)) {
    return false;
  }
  return HashKey(address.PlanKey) && HashKey(address.MaterialKey) &&
         ValidRoadPageAppearance(address.Appearance) &&
         address.MaterialIndex <= static_cast<uint32_t>(std::numeric_limits<int>::max()) &&
         std::isfinite(address.Anchor.LatitudeDeg) && std::isfinite(address.Anchor.LongitudeDeg) &&
         std::abs(address.Anchor.LatitudeDeg) <= kDegPerHalfTurn * 0.5 &&
         std::abs(address.Anchor.LongitudeDeg) <= kDegPerHalfTurn &&
         address.Parts.size() <= std::numeric_limits<uint32_t>::max() &&
         std::ranges::is_sorted(address.Parts, Before) &&
         std::ranges::adjacent_find(address.Parts) == address.Parts.end() &&
         std::ranges::all_of(address.Parts, [](const auto &part) {
           return part.Kind == RoadPagePartKind::Interval ||
                  part.Kind == RoadPagePartKind::Junction;
         });
}

bool AddressFields(BinaryValueWriter &out, const RoadPageAddress &address) {
  const bool appearance = std::holds_alternative<RoadPageAppearanceV2>(address.Appearance);
  auto format = appearance ? kAppearanceFormat : kLegacyFormat;
  if (address.PositionErrorM) { format = kDisplayFormat; }
  if (address.DisplayPolicy == RoadDisplayPolicy::UpperWithOriginalBodies) {
    format = kOriginalBodiesFormat;
  }
  if (address.Clustered) { format = kClusteredFormat; }
  const bool stored = address.DisplayPolicy == RoadDisplayPolicy::StoredSurfaceEnvelope;
  if (stored) { format = kStoredSurfaceFormat; }
  if (!out(format) || (stored && !out(address.Clustered)) || !out.Text(address.PlanKey) ||
      !out.Text(address.MaterialKey)) {
    return false;
  }
  if ((address.PositionErrorM || address.Clustered) && !out(static_cast<uint32_t>(appearance))) {
    return false;
  }
  if ((address.Clustered || stored) && !out(address.PositionErrorM.has_value())) { return false; }
  if (!WriteRoadPageAppearance(out, address.Appearance) ||
      !out(address.MaterialIndex,
           address.Anchor.LatitudeDeg,
           address.Anchor.LongitudeDeg,
           address.Level,
           static_cast<uint32_t>(address.Parts.size()))) {
    return false;
  }
  if (address.PositionErrorM && !out(*address.PositionErrorM)) { return false; }
  if ((format == kOriginalBodiesFormat || address.Clustered || stored) &&
      !out(address.DisplayPolicy)) {
    return false;
  }
  return std::ranges::all_of(address.Parts,
                             [&](const auto &part) { return out(part.Kind, part.Index); });
}

struct AddressFraming {
  uint64_t Format = 0;
  uint32_t Appearance = 0;
  bool Stored = false;
  bool Display = false;
};

std::optional<AddressFraming> ReadFraming(BinaryValueReader &in, RoadPageAddress &address) {
  uint64_t version = 0;
  if (!in(version) || (version != kLegacyFormat && version != kAppearanceFormat &&
                       version != kDisplayFormat && version != kOriginalBodiesFormat &&
                       version != kClusteredFormat && version != kStoredSurfaceFormat)) {
    return std::nullopt;
  }
  const bool stored = version == kStoredSurfaceFormat;
  address.Clustered = version == kClusteredFormat;
  if ((stored && !in(address.Clustered)) || !in.Text(address.PlanKey) ||
      !in.Text(address.MaterialKey)) {
    return std::nullopt;
  }
  uint32_t appearance = version == kAppearanceFormat ? 1 : 0;
  bool display = version == kDisplayFormat || version == kOriginalBodiesFormat;
  if ((display || address.Clustered || stored) && (!in(appearance) || appearance > 1)) {
    return std::nullopt;
  }
  if ((address.Clustered || stored) && !in(display)) { return std::nullopt; }
  return AddressFraming{
      .Format = version, .Appearance = appearance, .Stored = stored, .Display = display};
}

bool AddressFields(BinaryValueReader &in, RoadPageAddress &address) {
  const auto framing = ReadFraming(in, address);
  if (!framing) { return false; }
  uint32_t count = 0;
  if (!ReadRoadPageAppearance(in, framing->Appearance != 0, address.Appearance) ||
      !in(address.MaterialIndex,
          address.Anchor.LatitudeDeg,
          address.Anchor.LongitudeDeg,
          address.Level,
          count) ||
      count > in.In.Remaining() / (2 * sizeof(uint32_t))) {
    return false;
  }
  if (framing->Display) {
    double errorM = 0;
    if (!in(errorM)) { return false; }
    address.PositionErrorM = errorM;
  }
  if ((framing->Format == kOriginalBodiesFormat || address.Clustered || framing->Stored) &&
      !in(address.DisplayPolicy)) {
    return false;
  }
  if (framing->Format == kOriginalBodiesFormat &&
      address.DisplayPolicy != RoadDisplayPolicy::UpperWithOriginalBodies) {
    return false;
  }
  if (framing->Stored != (address.DisplayPolicy == RoadDisplayPolicy::StoredSurfaceEnvelope)) {
    return false;
  }
  address.Parts.resize(count);
  return std::ranges::all_of(address.Parts,
                             [&](auto &part) { return in(part.Kind, part.Index); }) &&
         ValidAddress(address);
}

bool StatsFields(BinaryValueWriter &out, const RoadMeshingStats &stats) {
  return out(std::array<uint64_t, 8>{stats.Pieces,
                                     stats.Cuts,
                                     stats.Refused,
                                     stats.Rejections.Fit,
                                     stats.Rejections.Rise,
                                     stats.Rejections.Bank,
                                     stats.Rejections.Sweep,
                                     stats.Rejections.TooShort});
}

bool StatsFields(BinaryValueReader &in, RoadMeshingStats &stats) {
  std::array<uint64_t, 8> values{};
  if (!in(values) || std::ranges::any_of(values, [](uint64_t value) {
        return value > std::numeric_limits<size_t>::max();
      })) {
    return false;
  }
  stats.Pieces = static_cast<size_t>(values[0]);
  stats.Cuts = static_cast<size_t>(values[1]);
  stats.Refused = static_cast<size_t>(values[2]);
  stats.Rejections = {.Fit = static_cast<size_t>(values[3]),
                      .Rise = static_cast<size_t>(values[4]),
                      .Bank = static_cast<size_t>(values[5]),
                      .Sweep = static_cast<size_t>(values[6]),
                      .TooShort = static_cast<size_t>(values[7])};
  return true;
}

bool DiagnosticsFields(BinaryValueWriter &out, const std::vector<RoadPageDiagnostic> &diagnostics) {
  if (diagnostics.size() > std::numeric_limits<uint32_t>::max() ||
      !out(static_cast<uint32_t>(diagnostics.size()))) {
    return false;
  }
  return std::ranges::all_of(diagnostics, [&](const auto &diagnostic) {
    return out(diagnostic.Part.Kind, diagnostic.Part.Index, diagnostic.Source, diagnostic.Empty) &&
           StatsFields(out, diagnostic.Stats);
  });
}

bool DiagnosticsFields(BinaryValueReader &in, std::vector<RoadPageDiagnostic> &diagnostics) {
  uint32_t count = 0;
  constexpr size_t recordBytes = 2 * sizeof(uint32_t) + 9 * sizeof(uint64_t) + sizeof(uint8_t);
  if (!in(count) || count > in.In.Remaining() / recordBytes) { return false; }
  diagnostics.resize(count);
  return std::ranges::all_of(diagnostics, [&](auto &diagnostic) {
    return in(diagnostic.Part.Kind, diagnostic.Part.Index, diagnostic.Source, diagnostic.Empty) &&
           StatsFields(in, diagnostic.Stats);
  });
}

bool ValidDiagnostics(const RoadPageAsset &page) {
  if (page.Address.PositionErrorM &&
      (page.Geometry.Swept.Refused != 0 || !page.Geometry.Diagnostics.empty() ||
       (!page.Address.Parts.empty() && page.Geometry.Mesh.Index.empty()))) {
    return false;
  }
  size_t refused = 0;
  std::optional<RoadPagePart> previous;
  for (const auto &diagnostic : page.Geometry.Diagnostics) {
    if (!std::ranges::binary_search(page.Address.Parts, diagnostic.Part, Before) ||
        (previous && !Before(*previous, diagnostic.Part)) ||
        (!diagnostic.Empty && diagnostic.Stats.Refused == 0) ||
        diagnostic.Stats.Refused > std::numeric_limits<size_t>::max() - refused) {
      return false;
    }
    refused += diagnostic.Stats.Refused;
    previous = diagnostic.Part;
  }
  return refused == page.Geometry.Swept.Refused;
}

bool ProductFields(BinaryValueWriter &out, const RoadPageAsset &page) {
  const auto &mesh = page.Geometry.Mesh;
  if (!(AddressFields(out, page.Address) && StatsFields(out, page.Geometry.Swept) &&
        DiagnosticsFields(out, page.Geometry.Diagnostics) && out.Array<float>(mesh.PositionM) &&
        out.Array<float>(mesh.NormalM) && out.Array<float>(mesh.ColourRgba) &&
        out.Array<uint32_t>(mesh.Index))) {
    return false;
  }
  if (!page.Address.Clustered) { return true; }
  if (page.Geometry.Clusters.size() > std::numeric_limits<uint32_t>::max() ||
      !out(static_cast<uint32_t>(page.Geometry.Clusters.size()))) {
    return false;
  }
  return std::ranges::all_of(page.Geometry.Clusters,
                             [&](const auto &cluster) { return ClusterFields(out, cluster); });
}

std::string_view MeshError(const RoadPageAsset &page) {
  if (page.Geometry.Parts != page.Address.Parts) {
    return "road page mesh ownership differs from its address";
  }
  const auto &mesh = page.Geometry.Mesh;
  const size_t vertices = mesh.PositionM.size() / 3;
  if (mesh.PositionM.size() % 3 != 0 || mesh.NormalM.size() != mesh.PositionM.size() ||
      vertices > std::numeric_limits<uint32_t>::max() || mesh.ColourRgba.size() / 4 != vertices ||
      mesh.ColourRgba.size() % 4 != 0 || mesh.Index.size() % 3 != 0 ||
      (!mesh.PositionM.empty() && page.Address.Parts.empty())) {
    return "road page mesh attribute sizes are inconsistent";
  }
  const auto finite = [](const auto &values) {
    return std::ranges::all_of(values, [](float value) { return std::isfinite(value); });
  };
  if (!finite(mesh.PositionM)) { return "road page mesh positions are nonfinite"; }
  if (!finite(mesh.NormalM)) { return "road page mesh normals are nonfinite"; }
  if (!finite(mesh.ColourRgba)) { return "road page mesh colours are nonfinite"; }
  if (!std::ranges::all_of(mesh.Index, [vertices](uint32_t index) { return index < vertices; })) {
    return "road page mesh indices exceed its vertex range";
  }
  if (page.Address.Clustered ? !ValidClusterRanges(page.Geometry.Clusters, mesh.Index.size())
                             : !page.Geometry.Clusters.empty()) {
    return "road page clusters differ from their declared index layout";
  }
  return {};
}
}

std::string RoadPageKey(const RoadPageAddress &address) {
  if (!ValidAddress(address)) { return {}; }
  BinaryValueWriter out(std::numeric_limits<uint32_t>::max());
  if (!AddressFields(out, address)) { return {}; }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

bool RoadPageHeaderFits(const RoadPageAddress &address, size_t bytesMost) {
  if (!ValidAddress(address)) { return false; }
  BinaryValueWriter out(bytesMost);
  RoadPageAsset empty;
  empty.Address = address;
  return ProductFields(out, empty);
}

std::expected<std::vector<uint8_t>, std::string_view> EncodeRoadPageAsset(const RoadPageAsset &page,
                                                                          size_t bytesMost) {
  if (!ValidAddress(page.Address)) { return std::unexpected("road page address is invalid"); }
  if (const auto error = MeshError(page); !error.empty()) { return std::unexpected(error); }
  if (!ValidDiagnostics(page)) {
    return std::unexpected("road page diagnostics disagree with its parts or refused total");
  }
  BinaryValueWriter out(bytesMost);
  if (!ProductFields(out, page)) {
    return std::unexpected("road page exceeds its native byte budget");
  }
  return std::move(out.Out).TakeBytes();
}

std::optional<RoadPageAsset> DecodeRoadPageAsset(std::span<const uint8_t> bytes, size_t bytesMost) {
  if (bytes.size() > bytesMost) { return std::nullopt; }
  BinaryValueReader in(bytes);
  RoadPageAsset page;
  auto &mesh = page.Geometry.Mesh;
  if (!AddressFields(in, page.Address)) { return std::nullopt; }
  page.Geometry.Parts = page.Address.Parts;
  if (!StatsFields(in, page.Geometry.Swept) || !DiagnosticsFields(in, page.Geometry.Diagnostics) ||
      !in.Array(mesh.PositionM) || !in.Array(mesh.NormalM) || !in.Array(mesh.ColourRgba) ||
      !in.Array(mesh.Index)) {
    return std::nullopt;
  }
  if (page.Address.Clustered) {
    uint32_t count = 0;
    if (!in(count) || count > in.In.Remaining() / kClusterBytes) { return std::nullopt; }
    page.Geometry.Clusters.resize(count);
    if (!std::ranges::all_of(page.Geometry.Clusters,
                             [&](auto &cluster) { return ClusterFields(in, cluster); })) {
      return std::nullopt;
    }
  }
  if (in.In.Remaining() != 0 || !MeshError(page).empty() || !ValidDiagnostics(page)) {
    return std::nullopt;
  }
  return page;
}

Box RoadPageBoundsEcef(const RoadPageAsset &page) {
  Box bounds;
  const auto frame = TangentFrame::At(page.Address.Anchor);
  const auto &positions = page.Geometry.Mesh.PositionM;
  for (size_t at = 0; at + 2 < positions.size(); at += 3) {
    const Vec3 local{{positions[at], positions[at + 1], positions[at + 2]}};
    bounds.Cover(frame.OriginEcef() + frame.EastEcef() * local[0] + frame.UpEcef() * local[1] -
                 frame.NorthEcef() * local[2]);
  }
  if (bounds.Empty()) { bounds.Cover(frame.OriginEcef()); }
  return bounds;
}
}
