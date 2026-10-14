#include "HipRoofCodec.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using StructureBinary::Reader;
using StructureBinary::Writer;
constexpr double kLargestStoredPrecisionM = 1e-6;
constexpr double kNormalLengthTolerance = 1e-12;

constexpr auto point = [](auto &archive, auto &value) {
  return archive.Number(value.EastM) && archive.Number(value.NorthM);
};

template <typename T, typename Visit>
bool BoundedList(Reader &archive, std::vector<T> &items, size_t most, Visit visit) {
  uint64_t count = 0;
  if (!archive.Number(count) || count > most || count > archive.Remaining ||
      count > archive.AllocationLeft / sizeof(T)) {
    return false;
  }
  archive.AllocationLeft -= static_cast<size_t>(count) * sizeof(T);
  items.resize(static_cast<size_t>(count));
  return std::ranges::all_of(items, [&](auto &item) { return visit(archive, item); });
}

constexpr auto facet = [](auto &archive, auto &value) {
  return point(archive, value.Gradient) && archive.Number(value.InterceptM) &&
         archive.Number(value.SourceRing) && archive.Number(value.SourceEdge);
};
}

bool HipRoofCodec::Fields(Writer &archive, const Product &value) {
  if (!Valid(value)) { return false; }
  uint8_t state = 0;
  if (value) { state = *value ? 1 : 2; }
  if (!archive.Number(state)) { return false; }
  if (state == 0) { return true; }
  if (state == 2) { return archive.Number(value->error()); }
  const auto &plan = **value;
  return point(archive, plan.Origin_) && archive.Number(plan.MaximumDistanceM_) &&
         archive.Number(plan.PrecisionM_) && archive.List(plan.Facets_, [](auto &held, auto &face) {
           return facet(held, face) && held.List(face.Ring, point);
         });
}

bool HipRoofCodec::Fields(Reader &archive, Product &value) {
  uint8_t state = 0;
  if (!archive.Number(state) || state > 2) { return false; }
  if (state == 0) {
    value.reset();
    return true;
  }
  if (archive.AllocationLeft < sizeof(std::expected<HipRoofPlan, HipRoofError>)) { return false; }
  archive.AllocationLeft -= sizeof(std::expected<HipRoofPlan, HipRoofError>);
  if (state == 2) {
    HipRoofError error{};
    if (!archive.Number(error) || error < HipRoofError::InvalidInput ||
        error > HipRoofError::InsufficientClearance) {
      return false;
    }
    value =
        std::make_shared<const std::expected<HipRoofPlan, HipRoofError>>(std::unexpected(error));
    return true;
  }
  HipRoofPlan plan;
  size_t cornersLeft = HipRoofPlan::Limits{}.Corners;
  if (!point(archive, plan.Origin_) || !archive.Number(plan.MaximumDistanceM_) ||
      !archive.Number(plan.PrecisionM_) ||
      !BoundedList(
          archive, plan.Facets_, HipRoofPlan::Limits{}.Facets, [&](auto &held, auto &face) {
            if (!facet(held, face) || !BoundedList(held, face.Ring, cornersLeft, point)) {
              return false;
            }
            cornersLeft -= face.Ring.size();
            return true;
          })) {
    return false;
  }
  value = std::make_shared<const std::expected<HipRoofPlan, HipRoofError>>(std::move(plan));
  return Valid(value);
}

bool HipRoofCodec::Valid(const Product &value) {
  if (!value) { return true; }
  if (!*value) {
    return value->error() >= HipRoofError::InvalidInput &&
           value->error() <= HipRoofError::InsufficientClearance;
  }
  const auto &plan = **value;
  if (!std::isfinite(plan.Origin_.EastM) || !std::isfinite(plan.Origin_.NorthM) ||
      !std::isfinite(plan.MaximumDistanceM_) || plan.MaximumDistanceM_ <= 0 ||
      !std::isfinite(plan.PrecisionM_) || plan.PrecisionM_ <= 0 ||
      plan.PrecisionM_ > kLargestStoredPrecisionM || plan.Facets_.empty() ||
      plan.Facets_.size() > HipRoofPlan::Limits{}.Facets) {
    return false;
  }
  size_t corners = 0;
  double maximum = 0;
  for (const auto &face : plan.Facets_) {
    if (face.Ring.size() < 3 || face.Ring.size() > HipRoofPlan::Limits{}.Corners - corners ||
        !std::isfinite(face.Gradient.EastM) || !std::isfinite(face.Gradient.NorthM) ||
        !std::isfinite(face.InterceptM) ||
        std::abs(std::hypot(face.Gradient.EastM, face.Gradient.NorthM) - 1) >
            kNormalLengthTolerance) {
      return false;
    }
    corners += face.Ring.size();
    for (const auto p : face.Ring) {
      const double distance = face.DistanceAt(p);
      if (!std::isfinite(p.EastM) || !std::isfinite(p.NorthM) || !std::isfinite(distance) ||
          distance < -plan.PrecisionM_) {
        return false;
      }
      maximum = std::max(maximum, distance);
    }
  }
  return std::abs(maximum - plan.MaximumDistanceM_) <= plan.PrecisionM_;
}

bool HipRoofCodec::BoundTo(const Product &value,
                           std::span<const EastNorth> exterior,
                           std::span<const std::vector<EastNorth>> holes) {
  if (!Valid(value)) { return false; }
  if (!value || !*value) { return true; }
  const auto &plan = **value;
  if (exterior.empty() || plan.Origin_ != exterior.front()) { return false; }
  for (const auto &face : plan.Facets_) {
    if (face.SourceRing > holes.size()) { return false; }
    const std::span<const EastNorth> ring =
        face.SourceRing == 0 ? exterior : holes[face.SourceRing - 1];
    if (face.SourceEdge >= ring.size()) { return false; }
    for (const size_t at : {face.SourceEdge, (face.SourceEdge + 1) % ring.size()}) {
      const EastNorth endpoint{.EastM = ring[at].EastM - plan.Origin_.EastM,
                               .NorthM = ring[at].NorthM - plan.Origin_.NorthM};
      if (std::abs(face.DistanceAt(endpoint)) > plan.PrecisionM_ ||
          !face.Contains(endpoint, plan.PrecisionM_)) {
        return false;
      }
    }
  }
  return true;
}

}
