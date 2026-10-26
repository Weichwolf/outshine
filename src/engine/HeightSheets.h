#ifndef OUTSHINE_ENGINE_HEIGHTSHEETS_H
#define OUTSHINE_ENGINE_HEIGHTSHEETS_H

#include <cstddef>
#include <string>

#include "GroundMesher.h"
#include "TerrainSeams.h"
#include <optional>
#include <expected>
#include "TangentFrame.h"
#include "streaming/TerrainResidency.h"

namespace outshine {

namespace Render {
class SceneRenderer;
}

class HeightSheets {
public:
  HeightSheets() = default;
  HeightSheets(const HeightSheets &other) = default;
  HeightSheets &operator=(const HeightSheets &) = delete;
  HeightSheets(HeightSheets &&) noexcept = default;
  HeightSheets &operator=(HeightSheets &&) noexcept = default;

  void Into(Render::SceneRenderer *renderer) noexcept { Residency_.Into(renderer); }

  void Framed(const TangentFrame &frame) {
    Frame_ = frame;
    Framed_ = true;
  }

  [[nodiscard]] bool Stitch(Patchwork &laid, std::string &error);
  [[nodiscard]] bool Hands(Patchwork &laid, std::string &error);
  [[nodiscard]] bool BeginResidency(const Patchwork &laid, std::string &error);
  [[nodiscard]] std::expected<bool, std::string> AdvanceResidency(const Patchwork &laid,
                                                                  size_t sheetsMost);

  [[nodiscard]] double LongestResidencyBatchMs() const noexcept {
    return Residency_.LongestBatchMs();
  }

  [[nodiscard]] double ResidencyFinalizeMs() const noexcept { return Residency_.FinalizeMs(); }

  void Clear();

  [[nodiscard]] size_t Standing() const { return Residency_.Standing(); }

  [[nodiscard]] size_t Instances() const { return Residency_.Instances(); }

  [[nodiscard]] size_t Flat() const { return Residency_.Flat(); }

  [[nodiscard]] std::optional<double> HeightMAt(LongitudeLatitude at) const noexcept {
    return Framed_ ? Residency_.HeightMAt(at) : std::nullopt;
  }

  [[nodiscard]] std::optional<std::array<double, 2>> AslBoundsM() const noexcept {
    return Framed_ ? Residency_.AslBoundsM() : std::nullopt;
  }

  [[nodiscard]] const TerrainResidency *PublishedSurface() const noexcept {
    return Framed_ && Residency_.HasPublishedSurface() ? &Residency_ : nullptr;
  }

  [[nodiscard]] uint64_t Digest() const;

  [[nodiscard]] size_t
  HeapBytes(TerrainHeapScope scope = TerrainHeapScope::Retained) const noexcept;

  [[nodiscard]] bool Occludes(const Ray &ray, float nearM, float distanceM) const noexcept {
    return Framed_ && Residency_.Occludes(ray, nearM, distanceM, Frame_);
  }

  [[nodiscard]] const Generators::Terrain::TerrainSeamMetrics &Seams() const { return Seams_; }

private:
  Generators::Terrain::TerrainSeamMetrics Seams_;
  TerrainResidency Residency_;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};

}
#endif
