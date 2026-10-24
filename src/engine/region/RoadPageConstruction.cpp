#include "RoadPageConstruction.h"
#include "ProfiledRoadMesher.h"
#include "Sha256.h"
#include <GeometryAsset.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
RoadPageConstruction::RoadPageConstruction(Tasks &pool,
                                           std::shared_ptr<Generators::PreparedRoadPages> pages)
    : Queue_(pool, std::move(pages), {.PendingMost = 1, .PageBytesMost = PageBytesMost}) {}

std::expected<std::unique_ptr<RoadPageConstruction>, std::string>
RoadPageConstruction::Begin(Tasks &pool,
                            std::shared_ptr<Generators::PreparedRoadPages> pages,
                            const Generators::Corridors::Job &plan,
                            const Generators::Corridors::Site &site,
                            const std::string &planKey) {
  if (!pages || planKey.size() != 64) {
    return std::unexpected("road construction needs its native cache and complete plan identity");
  }
  constexpr double kCellWidthM = 256;
  auto parts = Generators::Corridors::PlannedPages(plan, kCellWidthM, PartsMost);
  if (!parts) { return std::unexpected(std::string(parts.error())); }
  auto result =
      std::unique_ptr<RoadPageConstruction>(new RoadPageConstruction(pool, std::move(pages)));
  result->Pages_ = std::move(*parts);
  Geometry materials;
  if (!materials.addSurface("streets", Generators::Corridors::StreetMaterial(site.Materials))) {
    return std::unexpected("could not prepare the shared road material");
  }
  const auto encoded = EncodeGeometryAsset(materials, 4096);
  if (!encoded) { return std::unexpected("could not identify the shared road material"); }
  result->Address_.PlanKey = Sha256Hex("world-road-pages-1\n" + planKey);
  result->Address_.MaterialKey = Sha256Hex(encoded->data(), encoded->size());
  result->Address_.Anchor = site.Standing.Anchor();
  result->Manifest_.PlanKey = result->Address_.PlanKey;
  result->Manifest_.MaterialKey = result->Address_.MaterialKey;
  result->Manifest_.Anchor = result->Address_.Anchor;
  result->Manifest_.Materials = std::move(materials);
  return result;
}

std::expected<bool, std::string>
RoadPageConstruction::Advance(const Generators::Corridors::Job &plan,
                              const Generators::Corridors::Site &site) {
  if (Stage_ == Stage::Lookup || Stage_ == Stage::Generating) { return Collect(); }
  if (Next_ == Pages_.size()) { return true; }
  const auto requested = std::span(Pages_[Next_]);
  if (Stage_ == Stage::Request) {
    Address_.Parts.assign(requested.begin(), requested.end());
    PendingKey_ = Generators::RoadPageKey(Address_);
    if (!Queue_.RequestMetadata(Address_)) {
      return std::unexpected("road page lookup admission failed");
    }
    Stage_ = Stage::Lookup;
    return false;
  }
  auto recipe = Generators::Corridors::PageRecipe(plan, site, requested);
  if (!recipe) { return std::unexpected(std::string(recipe.error())); }
  const bool admitted =
      Queue_.Request(Address_,
                     {},
                     {},
                     [input = std::move(*recipe)](size_t, const std::stop_token &stop)
                         -> std::expected<Generators::RoadPageGeometry, std::string> {
                       const Generators::ProfiledRoadMesher mesher;
                       auto made = Generators::MeshRoadPage(mesher, input, stop);
                       if (!made) { return std::unexpected(std::string(made.error())); }
                       return std::move(*made);
                     });
  if (!admitted) { return std::unexpected("road page worker admission failed"); }
  ++MissRequests_;
  Stage_ = Stage::Generating;
  return false;
}

std::expected<bool, std::string> RoadPageConstruction::Collect() {
  auto completed = Queue_.Collect();
  if (completed.empty()) { return false; }
  auto &one = completed.front();
  if (completed.size() != 1 || one.Key != PendingKey_) {
    return std::unexpected("road page completion differs from the admitted world request");
  }
  if (!one.Result) { return std::unexpected(std::move(one.Result.error())); }
  if (!*one.Result) {
    if (Stage_ != Stage::Lookup) {
      return std::unexpected("generated road page is missing from its cache");
    }
    Stage_ = Stage::Generate;
    return false;
  }
  const AssetRecord *record = nullptr;
  if (Stage_ == Stage::Lookup) {
    record = std::get_if<AssetRecord>(&**one.Result);
  } else {
    const auto *loaded = std::get_if<Generators::PreparedRoadPages::Loaded>(&**one.Result);
    if (loaded == nullptr || !loaded->Page ||
        Generators::RoadPageKey(loaded->Page->Address) != PendingKey_) {
      return std::unexpected("road page references an unavailable material or different plan");
    }
    record = &loaded->Record;
  }
  if (record == nullptr || record->Key != PendingKey_ || record->Level != Address_.Level ||
      record->ByteCount == 0 || record->ByteCount > PageBytesMost) {
    return std::unexpected("road page metadata differs from its admitted native request");
  }
  NativeBytes_ += static_cast<size_t>(record->ByteCount);
  Manifest_.Pages.push_back({.Key = PendingKey_,
                             .Bounds = record->Bounds,
                             .ByteCount = record->ByteCount,
                             .Level = record->Level,
                             .Parts = Address_.Parts});
  ++Next_;
  ++PageCount_;
  Stage_ = Stage::Request;
  return false;
}

std::expected<Generators::RoadPageManifest, std::string> RoadPageConstruction::TakeManifest() {
  if (ManifestTaken_ || Stage_ != Stage::Request || Next_ != Pages_.size()) {
    return std::unexpected("road page manifest requires complete unpublished native products");
  }
  ManifestTaken_ = true;
  return std::move(Manifest_);
}
}
