#include "UrbanPrismPreparationWork.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {
void UrbanPrismPreparation::Work::Finish() {
  {
    const std::scoped_lock lock(Mutex);
    Complete.store(true, std::memory_order_release);
  }
  Landed.notify_all();
}

void UrbanPrismPreparation::Work::CacheFailure(AssetCacheError error, const char *message) {
  if (error == AssetCacheError::Busy) {
    Busy = true;
  } else {
    Value = std::unexpected(message);
  }
}

void UrbanPrismPreparation::Work::Read(const AssetCache &cache,
                                       const std::string &request,
                                       size_t bytesMost) {
  auto loaded = cache.LoadRequest(request, bytesMost);
  if (!loaded) {
    CacheFailure(loaded.error(), "could not read ready prism asset");
    return;
  }
  if (!*loaded) { return; }
  if ((**loaded).Record().Kind != "urban-prisms") {
    Value = std::unexpected("ready prism demand refers to another asset kind");
    return;
  }
  Measured.ReadBytes += (**loaded).Bytes().size();
  auto decoded = DecodeUrbanPrisms((**loaded).Bytes(), bytesMost);
  if (!decoded) {
    Value = std::unexpected(std::move(decoded.error()));
    return;
  }
  Value = Product(std::make_shared<UrbanPrisms>(std::move(*decoded)));
}

void UrbanPrismPreparation::Work::Build(AssetCache &cache,
                                        const std::string &request,
                                        size_t bytesMost,
                                        const std::stop_token &stop) {
  Read(cache, request, bytesMost);
  if (Busy || !Value || *Value) { return; }
  if (!Factory) {
    Value = std::unexpected("asset miss has no registered generator");
    return;
  }
  ++Measured.Generations;
  auto generated = Factory(bytesMost);
  Factory = {};
  if (stop.stop_requested()) {
    Value = std::unexpected("prism preparation canceled");
    return;
  }
  if (!generated) {
    Value = std::unexpected(std::move(generated.error()));
    return;
  }
  if (generated->Bytes.size() > bytesMost) {
    Value = std::unexpected("asset package exceeds its byte budget");
    return;
  }
  if (std::ranges::count_if(generated->Records, [&request](const AssetRecord &record) {
        return record.RequestKey && *record.RequestKey == request;
      }) != 1) {
    Value = std::unexpected("generator omitted the requested asset");
    return;
  }
  auto package =
      cache.PreparePublication(std::move(generated->Records), std::move(generated->Bytes));
  if (!package) {
    CacheFailure(package.error(), "could not prepare ready prism package");
    return;
  }
  ++Measured.PublicationPreparations;
  Package = std::move(*package);
  Next = Operation::Publish;
}

void UrbanPrismPreparation::Work::Publish(AssetCache &cache) {
  if (!Package) {
    Value = std::unexpected("prepared prism publication is missing");
    return;
  }
  ++Measured.PublicationAttempts;
  const auto published = cache.Publish(*Package);
  if (!published) {
    CacheFailure(published.error(), "asset cache storage failed");
    return;
  }
  Next = Operation::Restore;
}

void UrbanPrismPreparation::Work::Restore(const AssetCache &cache,
                                          const std::string &request,
                                          size_t bytesMost) {
  Read(cache, request, bytesMost);
  if (Busy || !Value) { return; }
  if (!*Value) {
    Value = std::unexpected("published asset package is unavailable");
    return;
  }
  Package.reset();
}

void UrbanPrismPreparation::Work::Run(const std::string &database,
                                      size_t bytesMost,
                                      const std::string &request,
                                      const std::stop_token &stop) {
  if (stop.stop_requested()) {
    Value = std::unexpected("prism preparation canceled");
  } else {
    auto cache = AssetCache::Open(database);
    if (!cache) {
      CacheFailure(cache.error(), "could not open ready prism cache");
    } else {
      while (!stop.stop_requested() && !Busy && Value && !*Value) {
        const auto previous = Next;
        switch (Next) {
          case Operation::Read: Read(**cache, request, bytesMost); break;
          case Operation::Build: Build(**cache, request, bytesMost, stop); break;
          case Operation::Publish: Publish(**cache); break;
          case Operation::Restore: Restore(**cache, request, bytesMost); break;
        }
        if (Next == previous) { break; }
      }
    }
  }
  if (stop.stop_requested()) {
    Busy = false;
    Value = std::unexpected("prism preparation canceled");
  }
  if (!Value || *Value) {
    Package.reset();
    Factory = {};
  }
}
}
