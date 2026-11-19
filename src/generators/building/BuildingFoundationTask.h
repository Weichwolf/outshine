#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGFOUNDATIONTASK_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGFOUNDATIONTASK_H

#include "BuildingPhysicalProduct.h"
#include "EarthworkStamp.h"
#include "TangentFrame.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace outshine {
class Tasks;
}

namespace outshine::Generators {
class BuildingFoundationTask {
public:
  struct Product {
    std::vector<EarthworkStamp> Stamps;
    double WorkerMs = 0;
  };

  using Result = std::expected<Product, std::string_view>;

  BuildingFoundationTask(Tasks &pool,
                         TangentFrame frame,
                         uint64_t generation,
                         std::vector<::outshine::Ground::BuildingProductLease> products);
  ~BuildingFoundationTask();
  BuildingFoundationTask(const BuildingFoundationTask &) = delete;
  BuildingFoundationTask &operator=(const BuildingFoundationTask &) = delete;

  [[nodiscard]] std::optional<Result> Take();
  [[nodiscard]] bool AwaitSlice(double seconds) const;
  [[nodiscard]] size_t VisitedFootprints() const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct State;
  Tasks &Pool_;
  std::shared_ptr<State> State_;
};
}

#endif
