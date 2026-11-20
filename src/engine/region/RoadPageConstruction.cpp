#include "RoadPageConstruction.h"
#include "RoadPageMeshProducer.h"
#include "RoadPageRecipeCodec.h"
#include "Sha256.h"
#include <GeometryProductCodec.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <cstdint>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kRoadLookupBatches = 4;
}

RoadPageConstruction::RoadPageConstruction(Tasks &pool,
                                           std::shared_ptr<Generators::PreparedRoadPages> pages,
                                           std::shared_ptr<AssetPipeline> publications)
    : BakesMost_(static_cast<size_t>(pool.Threads())),
      PendingMost_(BakesMost_ * kRoadLookupBatches),
      Queue_(pool,
             std::move(pages),
             {.PendingMost = PendingMost_, .PageBytesMost = PageBytesMost},
             std::move(publications)) {
  Pending_.reserve(PendingMost_);
}

std::expected<std::unique_ptr<RoadPageConstruction>, std::string>
RoadPageConstruction::Begin(Tasks &pool,
                            std::shared_ptr<Generators::PreparedRoadPages> pages,
                            const Generators::Corridors::Job &plan,
                            const Generators::Corridors::Site &site,
                            const std::string &planKey,
                            std::shared_ptr<AssetPipeline> publications) {
  if (!pages || planKey.size() != 64) {
    return std::unexpected("road construction needs its native cache and complete plan identity");
  }
  constexpr double kCellWidthM = 256;
  auto parts = Generators::Corridors::PlannedPages(plan, kCellWidthM, PartsMost);
  if (!parts) { return std::unexpected(std::string(parts.error())); }
  auto result = std::unique_ptr<RoadPageConstruction>(
      new RoadPageConstruction(pool, std::move(pages), std::move(publications)));
  result->Pages_ = std::move(*parts);
  Geometry materials;
  if (!materials.addSurface("streets", Generators::Corridors::StreetMaterial(site.Materials))) {
    return std::unexpected("could not prepare the shared road material");
  }
  const auto encoded = EncodeGeometryProduct(materials, 4096);
  if (!encoded) { return std::unexpected("could not identify the shared road material"); }
  result->Manifest_.PlanKey = Sha256Hex("world-road-pages-1\n" + planKey);
  result->Manifest_.MaterialKey = Sha256Hex(encoded->data(), encoded->size());
  result->Manifest_.Anchor = site.Standing.Anchor();
  result->Manifest_.Materials = std::move(materials);
  result->Manifest_.Pages.resize(result->Pages_.size());
  result->UniqueShapes_.reserve(result->Pages_.size());
  return result;
}

std::expected<bool, std::string>
RoadPageConstruction::Advance(const Generators::Corridors::Job &plan,
                              const Generators::Corridors::Site &site) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
  if (auto collected = Collect(); !collected) {
    return std::unexpected(std::move(collected.error()));
  }
  if (auto bakes = RequestBakes(plan, site, until); !bakes) {
    return std::unexpected(std::move(bakes.error()));
  }
  if (auto lookups = RequestLookups(plan, site, until); !lookups) {
    return std::unexpected(std::move(lookups.error()));
  }
  return PageCount_ == Pages_.size();
}

std::expected<RoadPageConstruction::Pending, std::string> RoadPageConstruction::PreparePage(
    const Generators::Corridors::Job &plan, const Generators::Corridors::Site &site, size_t index) {
  auto &parts = Pages_[index];
  auto recipe = Generators::Corridors::PageRecipe(plan, site, parts);
  if (!recipe) { return std::unexpected(std::string(recipe.error())); }
  auto &reference = Manifest_.Pages[index];
  auto shape = Generators::IdentifyRoadPageShape(*recipe, reference.Sources, PageBytesMost);
  if (!shape) { return std::unexpected("road recipe cannot be addressed by shape"); }
  if (UniqueShapes_.insert(*shape).second) {
    reference.ShapeKey = std::move(*shape);
  } else {
    reference.Sources.clear();
  }
  reference.Parts = std::move(parts);
  reference.Clustered = true;
  auto address = Generators::StoredRoadPageAddress(Manifest_, reference);
  auto key = Generators::RoadPageKey(address);
  return Pending{
      .Index = index, .Key = std::move(key), .Address = std::move(address), .Producer = {}};
}

std::expected<std::shared_ptr<const Generators::RoadPageProducer>, std::string>
RoadPageConstruction::PrepareBake(const Generators::Corridors::Job &plan,
                                  const Generators::Corridors::Site &site,
                                  const Pending &pending) const {
  const auto &reference = Manifest_.Pages[pending.Index];
  auto recipe = Generators::Corridors::PageRecipe(plan, site, reference.Parts);
  if (!recipe) { return std::unexpected(std::string(recipe.error())); }
  if (!reference.ShapeKey.empty()) {
    std::vector<uint64_t> sources;
    const auto shape = Generators::IdentifyRoadPageShape(*recipe, sources, PageBytesMost);
    if (!shape || *shape != reference.ShapeKey || sources != reference.Sources) {
      return std::unexpected("road recipe changed between lookup and bake");
    }
  }
  return std::make_shared<const Generators::RoadPageMeshProducer>(pending.Key, std::move(*recipe));
}

std::expected<void, std::string>
RoadPageConstruction::RequestLookups(const Generators::Corridors::Job &plan,
                                     const Generators::Corridors::Site &site,
                                     std::chrono::steady_clock::time_point until) {
  while (Requested_ < Pages_.size() && Pending_.size() < PendingMost_ &&
         std::chrono::steady_clock::now() < until) {
    auto pending = PreparePage(plan, site, Requested_);
    if (!pending) { return std::unexpected(std::move(pending.error())); }
    if (!Queue_.RequestMetadata(pending->Address)) {
      return std::unexpected("road page lookup admission failed");
    }
    Pending_.push_back(std::move(*pending));
    ++Requested_;
  }
  return {};
}

std::expected<void, std::string>
RoadPageConstruction::RequestBakes(const Generators::Corridors::Job &plan,
                                   const Generators::Corridors::Site &site,
                                   std::chrono::steady_clock::time_point until) {
  auto baking = static_cast<size_t>(std::ranges::count(Pending_, Stage::Baking, &Pending::Phase));
  for (auto &pending : Pending_) {
    if (baking >= BakesMost_ || std::chrono::steady_clock::now() >= until) { break; }
    if (pending.Phase != Stage::NeedsBake) { continue; }
    if (!pending.Producer) {
      auto producer = PrepareBake(plan, site, pending);
      if (!producer) { return std::unexpected(std::move(producer.error())); }
      pending.Producer = std::move(*producer);
    }
    const auto admitted = Queue_.RequestRecord({.Address = pending.Address,
                                                .Parent = {},
                                                .RequestKey = {},
                                                .NativeBytesMost = PageBytesMost},
                                               pending.Producer);
    if (!admitted) { return std::unexpected(admitted.error()); }
    if (!*admitted) { break; }
    pending.Producer.reset();
    pending.Phase = Stage::Baking;
    ++baking;
    ++MissRequests_;
  }
  return {};
}

std::expected<void, std::string> RoadPageConstruction::Collect() {
  for (auto &one : Queue_.Collect()) {
    const auto pending = std::ranges::find(Pending_, one.Key, &Pending::Key);
    if (pending == Pending_.end() || pending->Phase == Stage::NeedsBake) {
      return std::unexpected("road page completion differs from the admitted world request");
    }
    if (!one.Result) { return std::unexpected(std::move(one.Result.error())); }
    if (!*one.Result && pending->Phase == Stage::Lookup) {
      pending->Phase = Stage::NeedsBake;
      continue;
    }
    if (auto saved = SavePage(*pending, one); !saved) { return saved; }
    Pending_.erase(pending);
  }
  return {};
}

std::expected<void, std::string>
RoadPageConstruction::SavePage(Pending &pending, const RoadPageQueue::Completed &completed) {
  if (!*completed.Result) {
    return std::unexpected("generated road page is missing from its cache");
  }
  const auto *receipt = std::get_if<RoadPageQueue::Receipt>(&**completed.Result);
  const auto *record = receipt != nullptr && *receipt ? receipt->get() : nullptr;
  if (record == nullptr || record->Key != pending.Key || record->Level != pending.Address.Level ||
      record->ByteCount == 0 || record->ByteCount > PageBytesMost) {
    return std::unexpected("road page metadata differs from its admitted native request");
  }
  NativeBytes_ += static_cast<size_t>(record->ByteCount);
  auto &reference = Manifest_.Pages[pending.Index];
  reference.Key = std::move(pending.Key);
  reference.Bounds = record->Bounds;
  reference.ByteCount = record->ByteCount;
  ++PageCount_;
  return {};
}

std::expected<Generators::RoadPageManifest, std::string> RoadPageConstruction::TakeManifest() {
  if (ManifestTaken_ || PageCount_ != Pages_.size()) {
    return std::unexpected("road page manifest requires complete unpublished native products");
  }
  ManifestTaken_ = true;
  return std::move(Manifest_);
}
}
