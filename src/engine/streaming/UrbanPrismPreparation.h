#ifndef OUTSHINE_ENGINE_STREAMING_URBANPRISMPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_URBANPRISMPREPARATION_H

#include "Tasks.h"
#include "UrbanPrismCodec.h"
#include "content/AssetGeneration.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace outshine {
class UrbanPrismPreparation {
public:
  using Product = std::shared_ptr<const Generators::Osm::UrbanPrisms>;
  using Result = std::expected<Product, std::string>;
  using Captured = std::expected<AssetFactory, std::string>;
  using Capture = std::function<std::optional<Captured>(std::stop_token)>;

  struct Costs {
    double ReadWorkerMs = 0;
    double CaptureMainMs = 0;
    double BuildWorkerMs = 0;
    uint64_t ReadBytes = 0;
    uint32_t Generations = 0;
  };

  UrbanPrismPreparation(Tasks &pool,
                        std::string database,
                        std::string request,
                        size_t bytesMost = Generators::Osm::kUrbanPrismBytesMost);
  ~UrbanPrismPreparation();
  UrbanPrismPreparation(const UrbanPrismPreparation &) = delete;
  UrbanPrismPreparation &operator=(const UrbanPrismPreparation &) = delete;

  [[nodiscard]] const std::string &Key() const noexcept { return Request_; }

  [[nodiscard]] const Costs &Measurements() const noexcept { return Costs_; }

  [[nodiscard]] Result Advance(const Capture &capture = {});

  void Cancel() noexcept { Stop_.request_stop(); }

private:
  enum class Phase { Reading, Inputs, Building, Ready, Failed };
  enum class Operation { Read, Build };
  struct Work;
  void Post(Operation operation, AssetFactory factory = {});

  Tasks &Pool_;
  std::string Database_;
  std::string Request_;
  size_t BytesMost_ = 0;
  Phase Phase_ = Phase::Reading;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
  Result Result_;
  Costs Costs_;
};
}
#endif
