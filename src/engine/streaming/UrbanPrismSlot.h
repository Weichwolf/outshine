#ifndef OUTSHINE_ENGINE_STREAMING_URBANPRISMSLOT_H
#define OUTSHINE_ENGINE_STREAMING_URBANPRISMSLOT_H

#include "UrbanPrismPreparation.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace outshine {
class UrbanPrismSlot {
public:
  struct Counters {
    uint64_t Started = 0, Canceled = 0, Replaced = 0;
  };

  UrbanPrismSlot(Tasks &pool,
                 std::string database,
                 size_t bytesMost = Generators::Osm::kUrbanPrismBytesMost);
  [[nodiscard]] std::expected<bool, std::string>
  Advance(const std::string &request, const UrbanPrismPreparation::Capture &capture = {});
  void Clear();
  [[nodiscard]] bool InFlight() const noexcept;

  [[nodiscard]] const UrbanPrismPreparation::Product &Ready() const noexcept { return Ready_; }

  [[nodiscard]] const std::string &ReadyKey() const noexcept { return ReadyKey_; }

  [[nodiscard]] Counters Costs() const noexcept { return Costs_; }

private:
  void Retire();
  Tasks &Pool_;
  std::string Database_;
  size_t BytesMost_ = 0;
  std::string Wanted_, ReadyKey_;
  UrbanPrismPreparation::Product Ready_;
  std::unique_ptr<UrbanPrismPreparation> Job_, Retiring_;
  Counters Costs_;
};
}
#endif
