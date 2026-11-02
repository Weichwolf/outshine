#include "LitterPagePreparation.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
namespace {
using namespace Generators;
using Step = std::expected<AssetPipeline::LogicalStep, std::string>;
constexpr size_t kMetadataBytes = 2048;

enum class Domain { Request, Content };

struct Production {
  enum class Phase { Index, Load, GenerationAdmission, Generate, Publish, Deliver };
  std::shared_ptr<AssetCache> Cache;
  LitterPageAddress Address;
  std::string Key;
  LitterPageMiss OnMiss;
  size_t PackageBytes = 0, GenerationBytes = 0, ConsumerBytes = 0;
  Phase Stage = Phase::Index;
  AssetCache::ReadPlan ReadPlan;
  std::optional<PreparedAssetWrite> Publication;
  std::optional<AssetProduct> Delivery;

  Step Plan() {
    const auto plan = Cache->PlanRead(Key, PackageBytes);
    if (!plan) {
      if (plan.error() == AssetCacheError::Busy) { return std::optional<AssetProduct>{}; }
      return std::unexpected("litter asset read planning failed");
    }
    if (*plan) {
      ReadPlan = **plan;
      Stage = Phase::Load;
      return AssetPipeline::InputAdmission{.Bytes = ReadPlan.BufferBytesMost};
    }
    if (OnMiss == LitterPageMiss::Refuse) {
      return std::unexpected("litter asset miss has no registered generator");
    }
    Stage = Phase::Generate;
    return AssetPipeline::InputAdmission{.Bytes = GenerationBytes};
  }

  Step Load(Domain domain) {
    auto native =
        Cache->LoadValidated(Key, "ground-litter-page-r8", ReadPlan, [&](const CachedAsset &asset) {
          return DecodeLitterPage(Address, asset.Bytes()).has_value();
        });
    if (!native) {
      if (native.error() == AssetCacheError::Busy) { return std::optional<AssetProduct>{}; }
      if (native.error() != AssetCacheError::CapacityExceeded) {
        return std::unexpected("litter asset loading failed");
      }
    }
    if (!native || !*native) {
      Stage = Phase::Index;
      return AssetPipeline::InputAdmission{.Bytes = 0};
    }
    Delivery.emplace(std::move(**native), ConsumerBytes);
    if (domain == Domain::Request) { Delivery->SetRequestBinding(Key); }
    Stage = Phase::Deliver;
    return AssetPipeline::InputAdmission{.Bytes = Delivery->BackingBytes() + ConsumerBytes};
  }

  Step Generate(const std::stop_token &stop) {
    auto package = PrepareLitterPage(Address, PackageBytes, stop);
    if (!package) { return std::unexpected(std::move(package.error())); }
    if (stop.stop_requested()) { return std::unexpected("litter asset generation cancelled"); }
    auto prepared =
        Cache->PreparePublication(std::move(package->Records), std::move(package->Bytes));
    if (!prepared) { return std::unexpected("litter asset publication preparation failed"); }
    Publication.emplace(std::move(*prepared));
    Stage = Phase::Publish;
    return AssetPipeline::InputAdmission{.Bytes = Publication->CapacityBytes()};
  }

  Step Publish() {
    if (!Publication) { return std::unexpected("litter publication has no prepared product"); }
    const auto written = Cache->Publish(*Publication);
    if (!written) {
      if (written.error() == AssetCacheError::Busy) { return std::optional<AssetProduct>{}; }
      return std::unexpected("litter asset publication failed");
    }
    Publication.reset();
    Stage = Phase::Index;
    return AssetPipeline::InputAdmission{.Bytes = 0};
  }

  Step Advance(const std::stop_token &stop, Domain domain) {
    if (stop.stop_requested()) { return std::unexpected("litter asset request cancelled"); }
    switch (Stage) {
      case Phase::Index: return Plan();
      case Phase::Load: return Load(domain);
      case Phase::GenerationAdmission:
        Stage = Phase::Generate;
        return AssetPipeline::InputAdmission{.Bytes = GenerationBytes};
      case Phase::Generate: return Generate(stop);
      case Phase::Publish: return Publish();
      case Phase::Deliver: {
        if (!Delivery) { return std::unexpected("litter request has no native delivery"); }
        auto delivered = std::move(*Delivery);
        Delivery.reset();
        return std::optional<AssetProduct>(std::move(delivered));
      }
    }
    return std::unexpected("invalid litter request phase");
  }
};

}

std::expected<AssetPipeline::RequestTicket, AssetPublicationError>
RequestLitterPage(AssetPipeline &pipeline,
                  const AssetPipeline::Consumer &consumer,
                  std::shared_ptr<AssetCache> cache,
                  Generators::LitterPageAddress address,
                  LitterPageMiss onMiss,
                  size_t consumerBytesMost) {
  const auto key = LitterPageKey(address);
  const auto layout = PlanLitterPage(address);
  if (!cache || !key || !layout) { return std::unexpected(AssetPublicationError::InvalidRequest); }
  const size_t bytes = kLitterPageHeaderBytes + size_t{layout->Side} * layout->Side;
  const auto codec = AssetCache::PublicationBufferBytes(bytes);
  if (!codec ||
      *codec >
          std::numeric_limits<size_t>::max() - bytes - layout->WorkingBytesMost - kMetadataBytes ||
      consumerBytesMost > std::numeric_limits<size_t>::max() - bytes) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  auto work = std::make_shared<Production>();
  work->Cache = std::move(cache);
  work->Address = address;
  work->Key = *key;
  work->OnMiss = onMiss;
  work->PackageBytes = bytes;
  work->GenerationBytes = bytes + *codec + layout->WorkingBytesMost + kMetadataBytes;
  work->ConsumerBytes = consumerBytesMost;
  return pipeline.RequestLogical(
      consumer,
      {.Address = *key, .Generation = consumer.Generation(), .ReceiptBytesMost = kMetadataBytes},
      [work = std::move(work)](const std::stop_token &stop)
          -> std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure> {
        auto step = work->Advance(stop, Domain::Request);
        if (step && work->Stage == Production::Phase::Generate) {
          work->Stage = Production::Phase::GenerationAdmission;
          return AssetPipeline::ResolvedWork{
              .Key = *AssetProductKey::Parse(work->Key),
              .Produce = [work](const std::stop_token &next)
                  -> std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure> {
                return work->Advance(next, Domain::Content);
              }};
        }
        return step;
      });
}
}
