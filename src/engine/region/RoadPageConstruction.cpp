#include "RoadPageConstruction.h"
#include "RoadPageMeshProducer.h"
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

namespace outshine {
RoadPageConstruction::RoadPageConstruction(Tasks &pool,
                                           std::shared_ptr<Generators::PreparedRoadPages> pages,
                                           std::shared_ptr<AssetPipeline> publications)
    : PendingMost_(static_cast<size_t>(pool.Threads())),
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
  result->Address_.PlanKey = Sha256Hex("world-road-pages-1\n" + planKey);
  result->Address_.MaterialKey = Sha256Hex(encoded->data(), encoded->size());
  result->Address_.Anchor = site.Standing.Anchor();
  result->Address_.Clustered = true;
  result->Manifest_.PlanKey = result->Address_.PlanKey;
  result->Manifest_.MaterialKey = result->Address_.MaterialKey;
  result->Manifest_.Anchor = result->Address_.Anchor;
  result->Manifest_.Materials = std::move(materials);
  result->Manifest_.Pages.resize(result->Pages_.size());
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
  if (auto lookups = RequestLookups(until); !lookups) {
    return std::unexpected(std::move(lookups.error()));
  }
  return PageCount_ == Pages_.size();
}

Generators::RoadPageAddress RoadPageConstruction::AddressOf(size_t index) const {
  auto address = Address_;
  address.Parts = Pages_[index];
  return address;
}

std::expected<void, std::string>
RoadPageConstruction::RequestLookups(std::chrono::steady_clock::time_point until) {
  while (Requested_ < Pages_.size() && Pending_.size() < PendingMost_ &&
         std::chrono::steady_clock::now() < until) {
    auto address = AddressOf(Requested_);
    auto key = Generators::RoadPageKey(address);
    if (!Queue_.RequestMetadata(std::move(address))) {
      return std::unexpected("road page lookup admission failed");
    }
    Pending_.push_back({.Index = Requested_, .Key = std::move(key), .Producer = {}});
    ++Requested_;
  }
  return {};
}

std::expected<void, std::string>
RoadPageConstruction::RequestBakes(const Generators::Corridors::Job &plan,
                                   const Generators::Corridors::Site &site,
                                   std::chrono::steady_clock::time_point until) {
  for (auto &pending : Pending_) {
    if (std::chrono::steady_clock::now() >= until) { break; }
    if (pending.Phase != Stage::NeedsBake) { continue; }
    if (!pending.Producer) {
      auto recipe = Generators::Corridors::PageRecipe(plan, site, Pages_[pending.Index]);
      if (!recipe) { return std::unexpected(std::string(recipe.error())); }
      pending.Producer =
          std::make_shared<const Generators::RoadPageMeshProducer>(pending.Key, std::move(*recipe));
    }
    const auto admitted = Queue_.RequestRecord({.Address = AddressOf(pending.Index),
                                                .Parent = {},
                                                .RequestKey = {},
                                                .NativeBytesMost = PageBytesMost},
                                               pending.Producer);
    if (!admitted) { return std::unexpected(admitted.error()); }
    if (!*admitted) { break; }
    pending.Producer.reset();
    pending.Phase = Stage::Baking;
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
  if (record == nullptr || record->Key != pending.Key || record->Level != Address_.Level ||
      record->ByteCount == 0 || record->ByteCount > PageBytesMost) {
    return std::unexpected("road page metadata differs from its admitted native request");
  }
  NativeBytes_ += static_cast<size_t>(record->ByteCount);
  Manifest_.Pages[pending.Index] = {.Key = std::move(pending.Key),
                                    .Bounds = record->Bounds,
                                    .ByteCount = record->ByteCount,
                                    .Level = record->Level,
                                    .Parts = std::move(Pages_[pending.Index]),
                                    .Clustered = Address_.Clustered};
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
