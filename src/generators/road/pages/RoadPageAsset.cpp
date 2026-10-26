#include "RoadPageAsset.h"
#include "BinaryValueArchive.h"
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

bool HashKey(const std::string &key) {
  return key.size() == 64 && std::ranges::all_of(key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}

bool Before(const RoadPagePart &left, const RoadPagePart &right) {
  return left.Kind != right.Kind ? left.Kind < right.Kind : left.Index < right.Index;
}

bool ValidAddress(const RoadPageAddress &address) {
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
  const auto format = std::holds_alternative<LegacyRoadPageV1>(address.Appearance)
                          ? kLegacyFormat
                          : kAppearanceFormat;
  if (!out(format) || !out.Text(address.PlanKey) || !out.Text(address.MaterialKey) ||
      !WriteRoadPageAppearance(out, address.Appearance) ||
      !out(address.MaterialIndex,
           address.Anchor.LatitudeDeg,
           address.Anchor.LongitudeDeg,
           address.Level,
           static_cast<uint32_t>(address.Parts.size()))) {
    return false;
  }
  return std::ranges::all_of(address.Parts,
                             [&](const auto &part) { return out(part.Kind, part.Index); });
}

bool AddressFields(BinaryValueReader &in, RoadPageAddress &address) {
  uint64_t version = 0;
  uint32_t count = 0;
  if (!in(version) || (version != kLegacyFormat && version != kAppearanceFormat) ||
      !in.Text(address.PlanKey) || !in.Text(address.MaterialKey) ||
      !ReadRoadPageAppearance(in, version == kAppearanceFormat, address.Appearance) ||
      !in(address.MaterialIndex,
          address.Anchor.LatitudeDeg,
          address.Anchor.LongitudeDeg,
          address.Level,
          count) ||
      count > in.In.Remaining() / (2 * sizeof(uint32_t))) {
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
  return {};
}
}

std::string RoadPageKey(const RoadPageAddress &address) {
  if (!ValidAddress(address)) { return {}; }
  BinaryValueWriter out(std::numeric_limits<uint32_t>::max());
  if (!AddressFields(out, address)) { return {}; }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::expected<std::vector<uint8_t>, std::string_view> EncodeRoadPageAsset(const RoadPageAsset &page,
                                                                          size_t bytesMost) {
  if (!ValidAddress(page.Address)) { return std::unexpected("road page address is invalid"); }
  if (const auto error = MeshError(page); !error.empty()) { return std::unexpected(error); }
  if (!ValidDiagnostics(page)) {
    return std::unexpected("road page diagnostics disagree with its parts or refused total");
  }
  BinaryValueWriter out(bytesMost);
  const auto &mesh = page.Geometry.Mesh;
  if (!AddressFields(out, page.Address) || !StatsFields(out, page.Geometry.Swept) ||
      !DiagnosticsFields(out, page.Geometry.Diagnostics) || !out.Array<float>(mesh.PositionM) ||
      !out.Array<float>(mesh.NormalM) || !out.Array<float>(mesh.ColourRgba) ||
      !out.Array<uint32_t>(mesh.Index)) {
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
      !in.Array(mesh.Index) || in.In.Remaining() != 0 || !MeshError(page).empty() ||
      !ValidDiagnostics(page)) {
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
