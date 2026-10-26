#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFIELDQUERY_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFIELDQUERY_H

#include "RoadPortField.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::Generators {

enum class RoadPortQueryError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  OutsideDomain,
  InvalidField,
  UnqualifiedFrame,
  NoConvergence,
  GradeExceeded,
  Canceled
};

struct RoadPortQueryBudget {
  size_t Faces = 8192;
  size_t Queries = 4096;
  size_t FaceBoundsTests = 16777216;
};

struct RoadPortQueryWork {
  size_t Queries = 0;
  size_t FaceBoundsTests = 0;
};

class RoadPortFieldQuery {
public:
  [[nodiscard]] static std::expected<RoadPortFieldQuery, RoadPortQueryError>
  Make(const RoadPortField &field,
       std::span<const RoadPortFace> faces,
       RoadPortQueryBudget budget = {}) noexcept;
  [[nodiscard]] std::expected<RoadPortField::Sample, RoadPortQueryError> At(EastNorth at) noexcept;

  [[nodiscard]] RoadPortQueryWork Work() const noexcept { return Work_; }

private:
  RoadPortFieldQuery(const RoadPortField &field,
                     std::span<const RoadPortFace> faces,
                     RoadPortQueryBudget budget) noexcept;
  const RoadPortField *Field_;
  std::span<const RoadPortFace> Faces_;
  RoadPortQueryBudget Budget_;
  RoadPortQueryWork Work_;
};

}
#endif
