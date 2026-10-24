#include "PreparedRoadPages.h"
#include "ProfiledRoadMesher.h"
#include "Earth.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};
constexpr double kSpanM = 100;
constexpr double kSiblingEastM = 2 * kSpanM;

RoadPageAddress Address(uint32_t part) {
  return {.PlanKey = std::string(64, 'a'),
          .MaterialKey = std::string(64, 'b'),
          .MaterialIndex = 0,
          .Anchor = kAnchor,
          .Level = 0,
          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = part}}};
}

std::expected<RoadPageGeometry, std::string> Produce(double east, uint32_t edge = 7) {
  RoadPageRecipe recipe;
  recipe.Intervals.push_back(
      {.Edge = edge,
       .Lane = 0,
       .Stations = {{.EastM = east, .NorthM = 0, .GradeM = 0, .Node = 1},
                    {.EastM = east + kSpanM, .NorthM = 0, .GradeM = 0, .Node = 2}},
       .Surface = {.HalfWidthM = 4,
                   .Profile = RoadProfile::Kerbed,
                   .WearsLinear = {{0.5f, 0.5f, 0.5f}},
                   .Crossfall = 0,
                   .Form = RibbonForm::Surface,
                   .EndPorts = {}}});
  const ProfiledRoadMesher mesher;
  auto geometry = MeshRoadPage(mesher, recipe);
  if (!geometry) { return std::unexpected(std::string(geometry.error())); }
  return std::move(*geometry);
}

bool Same(const RoadMeshBuffers &left, const RoadMeshBuffers &right) {
  const auto equal = [](const auto &a, const auto &b) {
    return std::ranges::equal(std::as_bytes(std::span(a.data(), a.size())),
                              std::as_bytes(std::span(b.data(), b.size())));
  };
  return equal(left.PositionM, right.PositionM) && equal(left.NormalM, right.NormalM) &&
         equal(left.ColourRgba, right.ColourRgba) && equal(left.Index, right.Index);
}

void CheckFormat(const RoadPageAsset &page) {
  using namespace outshine::Test;
  const auto encoded = EncodeRoadPageAsset(page, kLimit);
  CHECK(encoded.has_value(), "a complete native page encodes within its supplied budget");
  if (!encoded) { return; }
  const auto decoded = DecodeRoadPageAsset(*encoded, kLimit);
  CHECK(decoded && Same(page.Geometry.Mesh, decoded->Geometry.Mesh) &&
            RoadPageKey(decoded->Address) == RoadPageKey(page.Address),
        "all page words and original scoped identities survive the native format");
  const auto overBudget = EncodeRoadPageAsset(page, encoded->size() - 1);
  CHECK(!overBudget && overBudget.error() == "road page exceeds its native byte budget" &&
            !DecodeRoadPageAsset(*encoded, encoded->size() - 1),
        "encoded byte limits apply before exposing a page");
  auto corrupt = *encoded;
  corrupt[0] ^= 1;
  CHECK(!DecodeRoadPageAsset(corrupt, kLimit), "unsupported page formats are rejected");
  corrupt = *encoded;
  corrupt.push_back(0);
  CHECK(!DecodeRoadPageAsset(corrupt, kLimit), "trailing page bytes are rejected");
  auto invalid = page;
  invalid.Geometry.Parts[0].Index += 1;
  const auto wrongOwner = EncodeRoadPageAsset(invalid, kLimit);
  CHECK(!wrongOwner && wrongOwner.error() == "road page mesh ownership differs from its address",
        "a producer cannot bind another part to the requested page");
  invalid = page;
  invalid.Geometry.Mesh.Index[0] = static_cast<uint32_t>(invalid.Geometry.Mesh.PositionM.size());
  const auto wrongIndex = EncodeRoadPageAsset(invalid, kLimit);
  CHECK(!wrongIndex && wrongIndex.error() == "road page mesh indices exceed its vertex range",
        "indices cannot reference another page's vertices");
  invalid = page;
  invalid.Geometry.Mesh.NormalM[0] = std::numeric_limits<float>::quiet_NaN();
  const auto invalidNormal = EncodeRoadPageAsset(invalid, kLimit);
  CHECK(!invalidNormal && invalidNormal.error() == "road page mesh normals are nonfinite",
        "nonfinite page words are rejected with their actual attribute");
  auto address = page.Address;
  address.Parts.push_back(address.Parts[0]);
  CHECK(RoadPageKey(address).empty(), "duplicate ownership cannot produce a stable page key");
  address = page.Address;
  address.PlanKey[0] = 'c';
  CHECK(RoadPageKey(address) != RoadPageKey(page.Address),
        "plan namespaces cannot alias local IDs");
  address = page.Address;
  address.MaterialKey[0] = 'c';
  CHECK(RoadPageKey(address) != RoadPageKey(page.Address), "material versions bind page identity");
  address = page.Address;
  ++address.Level;
  CHECK(RoadPageKey(address) != RoadPageKey(page.Address), "render levels are independently keyed");
}

class RefusingMesher final : public RoadMesher {
public:
  RoadMeshingStats Sweep(std::span<const RoadStation> /*stations*/,
                         RoadSweep /*surface*/,
                         RoadMeshBuffers & /*mesh*/) const override {
    return {.Pieces = 0, .Cuts = 0, .Refused = 1, .Rejections = {.Fit = 1}};
  }

  void Junction(std::span<const RoadGate> gates,
                RoadPlane plane,
                const Vec3f &wears,
                RoadMeshBuffers &mesh) const override {}
};

void CheckDiagnostics() {
  using namespace outshine::Test;
  RoadPageRecipe recipe;
  recipe.Intervals.push_back({.Edge = 7, .Lane = UINT32_MAX, .Stations = {}, .Surface = {}});
  recipe.Junctions.push_back(
      {.Index = 3, .Node = UINT64_MAX, .Gates = {}, .Plane = {}, .WearsLinear = {}});
  const RefusingMesher rejected;
  auto product = MeshRoadPage(rejected, recipe);
  CHECK(product && product->Diagnostics.size() == 2 && product->Swept.Refused == 1,
        "every refusal and empty native emission has an explicit original part diagnostic");
  if (!product) { return; }
  auto address = Address(7);
  address.Parts.push_back({.Kind = RoadPagePartKind::Junction, .Index = 3});
  RoadPageAsset page{.Address = address, .Geometry = std::move(*product)};
  const auto bytes = EncodeRoadPageAsset(page, kLimit);
  const auto decoded = bytes ? DecodeRoadPageAsset(*bytes, kLimit) : std::nullopt;
  CHECK(decoded && decoded->Geometry.Diagnostics[0].Source == UINT32_MAX &&
            decoded->Geometry.Diagnostics[0].Stats.Rejections.Fit == 1 &&
            decoded->Geometry.Diagnostics[1].Source == UINT64_MAX &&
            decoded->Geometry.Diagnostics[1].Empty,
        "reason and full-width source/node IDs survive native page storage");
  page.Geometry.Diagnostics.erase(page.Geometry.Diagnostics.begin());
  CHECK(!EncodeRoadPageAsset(page, kLimit),
        "aggregate refusals cannot hide their source diagnostics");
}

void CheckCache(const std::filesystem::path &root, const RoadPageAsset &first) {
  using namespace outshine::Test;
  auto opened = PreparedRoadPages::Open(root.string());
  CHECK(opened.has_value(), "road pages use the existing assets.sqlite schema");
  if (!opened) { return; }
  size_t factories = 0;
  const auto factory = [&](size_t limit) -> std::expected<RoadPageGeometry, std::string> {
    ++factories;
    CHECK(limit == kLimit, "the native producer receives the unchanged page budget");
    return Produce(0);
  };
  const std::string parent(64, 'c');
  const std::string request(64, 'd');
  auto &pages = **opened;
  const auto miss = pages.Load(first.Address, kLimit);
  CHECK(miss && !*miss, "absent pages remain ordinary misses");
  CHECK(!pages.Resolve(first.Address, parent, "invalid", kLimit, factory) && factories == 0,
        "invalid demand identities fail before mesh generation");
  CHECK(!pages.Resolve(first.Address, parent, request, 1, factory) && factories == 0,
        "an impossible header budget fails before mesh generation");
  auto cold = pages.Resolve(first.Address, parent, request, kLimit, factory);
  CHECK(cold && factories == 1 && Same(cold->Page->Geometry.Mesh, first.Geometry.Mesh),
        "a miss builds and reads the exact finished native page");
  if (!cold) { return; }
  const auto hit = pages.Resolve(first.Address, parent, request, kLimit, {});
  CHECK(hit && factories == 1 && hit->Record.Key == cold->Record.Key &&
            hit->Record.Package == cold->Record.Package,
        "a hit loads without any source or mesh factory");
  CHECK(hit && hit->Page.get() != cold->Page.get() &&
            hit->Page->Geometry.Mesh.PositionM.data() != cold->Page->Geometry.Mesh.PositionM.data(),
        "independent loads exclusively own distinct decoded pages and mesh allocations");
  const auto siblingAddress = Address(8);
  const auto sibling = pages.Resolve(
      siblingAddress, parent, {}, kLimit, [](size_t) { return Produce(kSiblingEastM, 8); });
  CHECK(sibling && sibling->Record.Package != cold->Record.Package &&
            sibling->Page->Address.MaterialKey == cold->Page->Address.MaterialKey,
        "whole page blobs are independently compressed with one shared material reference");
  if (!sibling) { return; }
  auto cache = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(cache && (*cache)->Find(cold->Record.Key) && (*cache)->Find(sibling->Record.Key),
        "the existing cache indexes each independent finished product");
  if (!cache) { return; }
  const auto bound = (*cache)->FindRequest(request);
  CHECK(bound && *bound && (**bound).Key == cold->Record.Key,
        "demand binding points to the complete page");
  auto fresh = PreparedRoadPages::Open(root.string());
  CHECK(fresh.has_value(), "a fresh page connection opens");
  if (!fresh) { return; }
  const auto restored = fresh
                            ? (*fresh)->LoadRequest(request, kLimit)
                            : std::expected<std::optional<PreparedRoadPages::Loaded>, std::string>{
                                  std::unexpected("fresh page cache unavailable")};
  CHECK(restored && *restored && Same((**restored).Page->Geometry.Mesh, first.Geometry.Mesh),
        "a fresh service restores a bound demand without source or plan inputs");
  const auto direct = (*fresh)->LoadKey(cold->Record.Key, kLimit);
  CHECK(direct && *direct && Same((**direct).Page->Geometry.Mesh, first.Geometry.Mesh),
        "a product key from metadata restores the exact native page without an address");
  const auto absent = (*fresh)->LoadKey(std::string(64, 'f'), kLimit);
  CHECK(absent && !*absent, "a missing product key is a normal native-cache miss");
  CHECK(!(*fresh)->LoadKey("", kLimit) && !(*fresh)->LoadKey(std::string(64, 'g'), kLimit),
        "invalid direct product identities fail before cache loading");
  const std::string reboundRequest(64, 'e');
  const auto rebound = pages.Resolve(first.Address, parent, reboundRequest, kLimit, {});
  const auto reboundMetadata = (*cache)->FindRequest(reboundRequest);
  CHECK(rebound && rebound->Record.RequestKey == reboundRequest && reboundMetadata &&
            *reboundMetadata && (**reboundMetadata).Key == rebound->Record.Key,
        "a rebound cache hit returns the metadata snapshot actually published for this demand");
  auto retained = std::move(cold->Page);
  const auto removed = (*cache)->Remove(cold->Record.Key);
  const auto afterRemoval = (*cache)->Find(cold->Record.Key);
  CHECK(removed && afterRemoval && !*afterRemoval, "the indexed product can be evicted");
  opened->reset();
  fresh->reset();
  cache->reset();
  CHECK(Same(retained->Geometry.Mesh, first.Geometry.Mesh),
        "an owned page lease survives cache closure and eviction");
  retained->Geometry.Mesh.PositionM[0] += 1;
  CHECK(hit && direct && *direct && Same(hit->Page->Geometry.Mesh, first.Geometry.Mesh) &&
            Same((**direct).Page->Geometry.Mesh, first.Geometry.Mesh),
        "the exclusive owner may change its page without mutating any other decoded lease");
}
}

int main() {
  using namespace outshine::Test;
  auto geometry = Produce(0);
  CHECK(geometry && !geometry->Mesh.Index.empty(), "the test uses actual native road generation");
  if (!geometry || geometry->Mesh.Index.empty()) { return Report(); }
  const RoadPageAsset page{.Address = Address(7), .Geometry = std::move(*geometry)};
  CheckFormat(page);
  CheckDiagnostics();
  const auto bounds = RoadPageBoundsEcef(page);
  CHECK(!bounds.Empty() && std::isfinite(bounds.Min[0]) && std::isfinite(bounds.Max[2]),
        "native page metadata uses finite world ECEF bounds");
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-road-page-cache-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  CheckCache(root, page);
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the owned temporary page cache is cleaned");
  return Report();
}
