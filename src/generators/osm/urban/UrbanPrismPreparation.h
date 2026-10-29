#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMPREPARATION_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMPREPARATION_H

#include "Tasks.h"
#include "UrbanPrismCodec.h"
#include "AssetGeneration.h"
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace outshine::Generators::Osm {
class UrbanPrismPreparation {
public:
  using Product = std::shared_ptr<const UrbanPrisms>;
  using Result = std::expected<Product, std::string>;
  using Captured = std::expected<AssetFactory, std::string>;
  using Capture = std::function<std::optional<Captured>(std::stop_token)>;

  struct Costs {
    double ReadWorkerMs = 0;
    double CaptureMainMs = 0;
    double BuildWorkerMs = 0;
    uint64_t ReadBytes = 0;
    uint32_t Generations = 0, PublicationPreparations = 0;
    uint64_t PublicationAttempts = 0;
    // Current prepared-package capacity; captured input/ready products are separate.
    size_t PublicationBytes = 0;
  };

  UrbanPrismPreparation(Tasks &pool,
                        std::string database,
                        std::string request,
                        size_t bytesMost = kUrbanPrismBytesMost);
  ~UrbanPrismPreparation();
  UrbanPrismPreparation(const UrbanPrismPreparation &) = delete;
  UrbanPrismPreparation &operator=(const UrbanPrismPreparation &) = delete;

  [[nodiscard]] const std::string &Key() const noexcept { return Request_; }

  [[nodiscard]] const Costs &Measurements() const noexcept { return Costs_; }

  [[nodiscard]] Result Advance(const Capture &capture = {}, bool allowWork = true);

  [[nodiscard]] bool InFlight() const noexcept;
  // Deferred work retains its original admission even with no worker running.
  [[nodiscard]] bool HoldsAdmission() const noexcept;
  [[nodiscard]] bool AwaitProgress(double seconds) const;

  void Cancel() noexcept;

private:
  enum class Phase { Reading, Inputs, Building, Ready, Failed };
  enum class Operation { Read, Build, Publish, Restore };
  struct Work;
  void Post(Operation operation, AssetFactory factory = {});
  void Collect();
  void Defer();

  Tasks &Pool_;
  std::string Database_;
  std::string Request_;
  size_t BytesMost_ = 0;
  Phase Phase_ = Phase::Reading;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
  Result Result_;
  Costs Costs_;
  std::optional<PreparedAssetWrite> Pending_;
  AssetFactory Factory_;
  Operation Next_ = Operation::Read;
  unsigned Retries_ = 0;
  std::chrono::steady_clock::time_point RetryAt_;
};
}
#endif
