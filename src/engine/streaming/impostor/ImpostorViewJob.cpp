#include "ImpostorViewJob.h"
#include <concepts>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
namespace {
using Views = Content::ImpostorViewAssets;
using Result = std::expected<std::optional<AssetRecord>, std::string>;

template <class Product> std::string Key(const Views &views, const Product &product) {
  if constexpr (std::same_as<Product, ImpostorViewJob::SurfaceView>) {
    return views.Key(product.Selected);
  } else {
    return views.FieldKey(product.Selected);
  }
}

template <class Product>
std::expected<bool, AssetCacheError>
Loaded(const Views &views, const Product &product, size_t limit) {
  const auto loaded = [&] {
    if constexpr (std::same_as<Product, ImpostorViewJob::SurfaceView>) {
      return views.Load(product.Selected, limit);
    } else {
      return views.LoadField(product.Selected, limit);
    }
  }();
  if (!loaded) { return std::unexpected(loaded.error()); }
  return loaded->has_value();
}

template <class Product>
std::expected<Views::ProductWrite, std::string>
Produce(const Views &views, const Product &product, size_t bytesMost, const std::stop_token &stop) {
  if (stop.stop_requested() || !product.Generate) {
    return std::unexpected(stop.stop_requested() ? "impostor view request cancelled"
                                                 : "impostor view miss has no producer");
  }
  auto generated = product.Generate(product.Selected, bytesMost, stop);
  if (!generated) { return std::unexpected(std::move(generated.error())); }
  if (stop.stop_requested()) { return std::unexpected("impostor view request cancelled"); }
  auto prepared = [&] {
    if constexpr (std::same_as<Product, ImpostorViewJob::SurfaceView>) {
      return views.Prepare(product.Selected, std::move(*generated), bytesMost);
    } else {
      return views.PrepareField(product.Selected, *generated, bytesMost);
    }
  }();
  if (!prepared) { return std::unexpected("impostor view preparation failed"); }
  return std::move(*prepared);
}

struct Work {
  enum class Phase { Load, Publish, Receipt, Ready };
  ImpostorViewJob Job;
  std::optional<Views> Metadata;
  std::optional<Views::ProductWrite> Publication;
  std::optional<AssetRecord> Ready;
  Phase Current = Phase::Load;

  explicit Work(ImpostorViewJob job) : Job(std::move(job)) {}

  std::expected<bool, std::string> MetadataCurrent() {
    if (!Metadata) {
      auto opened = Views::Open(*Job.Cache, Job.Provenance);
      if (!opened) {
        if (opened.error() == AssetCacheError::Busy) { return false; }
        return std::unexpected("impostor view metadata unavailable");
      }
      if (!*opened) { return std::unexpected("impostor view metadata unavailable"); }
      Metadata.emplace(std::move(**opened));
      if (std::visit([&](const auto &product) { return Key(*Metadata, product); }, Job.Product) !=
          Job.ProductKey) {
        return std::unexpected("impostor view metadata changed before production");
      }
    } else {
      const auto current = Job.Cache->Find(Metadata->Receipt().Key);
      if (!current) {
        if (current.error() == AssetCacheError::Busy) { return false; }
        return std::unexpected("impostor view metadata unavailable");
      }
      if (!*current || **current != Metadata->Receipt()) {
        return std::unexpected("impostor view metadata changed before publication");
      }
    }
    return true;
  }

  Result Load(const Views &metadata, const std::stop_token &stop) {
    const auto loaded = std::visit(
        [&](const auto &product) { return Loaded(metadata, product, Job.BytesMost); }, Job.Product);
    if (!loaded) {
      if (loaded.error() == AssetCacheError::Busy) { return std::optional<AssetRecord>{}; }
      return std::unexpected("impostor view acquisition failed");
    }
    if (*loaded) {
      Current = Phase::Receipt;
      return Receipt(metadata);
    }
    auto prepared = std::visit(
        [&](const auto &product) { return Produce(metadata, product, Job.BytesMost, stop); },
        Job.Product);
    if (!prepared) { return std::unexpected(prepared.error()); }
    Publication.emplace(std::move(*prepared));
    Current = Phase::Publish;
    return std::optional<AssetRecord>{};
  }

  Result Receipt(const Views &metadata) {
    auto record = Job.Cache->Find(Job.ProductKey);
    if (!record) {
      if (record.error() == AssetCacheError::Busy) { return std::optional<AssetRecord>{}; }
      return std::unexpected("impostor view receipt unavailable");
    }
    if (!*record) {
      Current = Phase::Load;
      return std::optional<AssetRecord>{};
    }
    if ((**record).ByteCount > Job.BytesMost || (**record).Parent != metadata.Receipt().Key ||
        (**record).Bounds != metadata.Receipt().Bounds) {
      return std::unexpected("impostor view receipt does not match admitted metadata");
    }
    Ready = std::move(*record);
    Current = Phase::Ready;
    return Ready;
  }

  Result Step(const std::stop_token &stop) {
    if (stop.stop_requested()) {
      Publication.reset();
      return std::unexpected("impostor view request cancelled");
    }
    if (!Job.Cache || Job.Provenance.empty() || Job.ProductKey.empty() || Job.BytesMost == 0) {
      return std::unexpected("invalid impostor view job");
    }
    const auto current = MetadataCurrent();
    if (!current) { return std::unexpected(current.error()); }
    if (!*current) { return std::optional<AssetRecord>{}; }
    if (!Metadata) { return std::unexpected("impostor view metadata owner unavailable"); }
    const auto &metadata = *Metadata;
    switch (Current) {
      case Phase::Load: return Load(metadata, stop);
      case Phase::Publish: {
        if (!Publication) { return std::unexpected("impostor view publication owner unavailable"); }
        const auto published = metadata.Publish(*Publication);
        if (!published) {
          if (published.error() == AssetCacheError::Busy) { return std::optional<AssetRecord>{}; }
          return std::unexpected("impostor view publication failed");
        }
        Ready = *published;
        Publication.reset();
        Current = Phase::Ready;
        return Ready;
      }
      case Phase::Receipt: return Receipt(metadata);
      case Phase::Ready:
        if (!Ready) { return std::unexpected("impostor view ready receipt unavailable"); }
        return Ready;
    }
    std::unreachable();
  }
};
}

ImpostorViewJob::Resolver ImpostorViewJob::Prepare() const {
  auto work = std::make_shared<Work>(*this);
  return [work = std::move(work)](const std::stop_token &stop) { return work->Step(stop); };
}
}
