#include "RoadPageConstruction.h"
#include "ProfiledRoadMesher.h"
#include "Sha256.h"
#include <content/GeometryAsset.h>
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
  auto parts = Generators::Corridors::PlannedParts(plan);
  if (!parts) { return std::unexpected(std::string(parts.error())); }
  auto result =
      std::unique_ptr<RoadPageConstruction>(new RoadPageConstruction(pool, std::move(pages)));
  result->Parts_ = std::move(*parts);
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
  if (Next_ == Parts_.size()) { return true; }
  const auto count = std::min(PartsMost, Parts_.size() - Next_);
  const auto requested = std::span(Parts_).subspan(Next_, count);
  if (Stage_ == Stage::Request) {
    Address_.Parts.assign(requested.begin(), requested.end());
    PendingKey_ = Generators::RoadPageKey(Address_);
    if (!Queue_.RequestCached(PendingKey_)) {
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
  const auto &loaded = **one.Result;
  const auto &page = loaded.Page;
  if (!page || Generators::RoadPageKey(page->Address) != PendingKey_ ||
      page->Address.MaterialKey != Address_.MaterialKey || page->Address.MaterialIndex != 0) {
    return std::unexpected("road page references an unavailable material or different plan");
  }
  NativeBytes_ += loaded.NativeBytes;
  Meshing_ += page->Geometry.Swept;
  Manifest_.Pages.push_back({.Key = PendingKey_,
                             .Bounds = loaded.Record.Bounds,
                             .ByteCount = loaded.NativeBytes,
                             .Level = page->Address.Level,
                             .Parts = page->Address.Parts});
  Next_ += Address_.Parts.size();
  ++PageCount_;
  Stage_ = Stage::Request;
  return false;
}

std::expected<Generators::RoadPageManifest, std::string> RoadPageConstruction::TakeManifest() {
  if (ManifestTaken_ || Stage_ != Stage::Request || Next_ != Parts_.size()) {
    return std::unexpected("road page manifest requires complete unpublished native products");
  }
  ManifestTaken_ = true;
  return std::move(Manifest_);
}
}
