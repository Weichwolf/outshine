#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSOURCEVIEW_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSOURCEVIEW_H

#include "BuildingPlaneCapture.h"
#include "BuildingSourceBudget.h"
#include "PreparedStructureTile.h"
#include "Geometry.h"
#include "Geodesy.h"
#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators {
struct BuildingCaptureFrame {
  EnuAxes Axes;
  Vec3 Origin;
  [[nodiscard]] bool Valid() const noexcept;
  [[nodiscard]] Vec3 Local(const Vec3 &point) const noexcept;
  [[nodiscard]] Vec3 SourceDirection(const Vec3 &direction) const noexcept;
};

class BuildingSourceView {
public:
  BuildingSourceView(BuildingSourceView &&other) noexcept = default;
  BuildingSourceView &operator=(BuildingSourceView &&other) noexcept = default;
  BuildingSourceView(const BuildingSourceView &) = delete;
  BuildingSourceView &operator=(const BuildingSourceView &) = delete;

  struct Error {
    BuildingPlaneCapture::ErrorKind Kind = BuildingPlaneCapture::ErrorKind::InvalidInput;
    std::optional<uint32_t> Source = std::nullopt;
    std::string Message;
  };

  struct Product {
    Geometry Mesh;
    uint64_t Rays = 0, Covered = 0, SourceTests = 0;
    double SamplingMs = 0, ConversionMs = 0;
  };

  // Open uses prepared selection headers only; the retained base owns lazy source forms.
  [[nodiscard]] static std::expected<BuildingSourceView, Error>
  Open(std::shared_ptr<const PreparedStructureTile> base,
       std::span<const uint32_t> cells,
       BuildingCaptureFrame frame);

  [[nodiscard]] const Box &Bounds() const noexcept { return Bounds_; }

  [[nodiscard]] std::span<const uint32_t> Sources() const noexcept { return Sources_; }

  [[nodiscard]] std::span<const uint32_t> Unsupported() const noexcept { return Unsupported_; }

  [[nodiscard]] std::expected<Geometry, Error>
  Originals(BuildingSourceBudget budget,
            const std::array<Material, 3> &wallRoofMetreMaterials,
            const std::stop_token &stop = {}) const;

  [[nodiscard]] std::expected<Product, Error>
  Capture(BuildingPlaneCapture::View localView,
          BuildingPlaneCapture::Budget budget,
          const std::array<Material, 3> &wallRoofMetreMaterials,
          const std::stop_token &stop = {}) const;

private:
  BuildingSourceView(std::shared_ptr<const PreparedStructureTile> base,
                     BuildingCaptureFrame frame,
                     BuildingPlaneCapture producer,
                     Box bounds,
                     std::vector<uint32_t> sources,
                     std::vector<uint32_t> unsupported);
  std::shared_ptr<const PreparedStructureTile> Base_;
  BuildingCaptureFrame Frame_;
  BuildingPlaneCapture Producer_;
  Box Bounds_;
  std::vector<uint32_t> Sources_;
  std::vector<uint32_t> Unsupported_;
};
}
#endif
