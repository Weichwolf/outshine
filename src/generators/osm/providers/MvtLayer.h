#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_MVTLAYER_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_MVTLAYER_H

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators::Osm {

class MvtLayer {
public:
  struct Ring {
    uint32_t First = 0, Count = 0;
    bool Exterior = true;
  };

  struct Feature {
    uint32_t FirstRing = 0, RingCount = 0;
    uint32_t FirstTag = 0, TagCount = 0;
    int Type = 0;
    bool HasContentDigest = false;
    std::optional<uint64_t> ProviderFeatureId;
    // Original feature message digest, independent of native array placement.
    std::array<uint8_t, 32> ContentDigest{};
  };

  enum class ParseError { MissingLayer, InvalidTile, UnsupportedVersion, CapacityExceeded };

  struct AllocationPlan {
    size_t Features = 0, Rings = 0, Points = 0, Tags = 0, Keys = 0, Values = 0;
    size_t TextBytes = 0;
    size_t ScratchTags = 0, ScratchGeometry = 0;
    size_t HeapBytesMost = 0, PeakBytesMost = 0;
  };

  // Borrows input and counts selected-layer storage without heap allocation.
  // Geometry validity is checked during Parse; command counts never size allocations.
  [[nodiscard]] static std::expected<AllocationPlan, ParseError>
  Plan(std::span<const uint8_t> bytes, std::string_view layer);

  [[nodiscard]] std::expected<void, ParseError>
  Parse(std::span<const uint8_t> bytes,
        std::string_view layer,
        size_t bytesMost = std::numeric_limits<size_t>::max());

  [[nodiscard]] int Extent() const { return Extent_; }

  [[nodiscard]] const std::vector<Feature> &Features() const { return Features_; }

  [[nodiscard]] const std::vector<Ring> &Rings() const { return Rings_; }

  [[nodiscard]] const std::vector<int32_t> &Points() const { return Points_; }

  [[nodiscard]] size_t HeapBytes() const;

  [[nodiscard]] double NumberTag(const Feature &feature, const char *key, double fallback) const;
  [[nodiscard]] std::string_view StringTag(const Feature &feature, const char *key) const;

  struct Tag {
    std::string_view Key, String;
    double Number = 0.0;
    bool IsNumber = false;
  };

  [[nodiscard]] static uint32_t TagCount(const Feature &f) { return f.TagCount / 2; }

  [[nodiscard]] Tag TagAt(const Feature &f, uint32_t i) const;

private:
  [[nodiscard]] std::expected<void, ParseError>
  Decode(std::span<const uint8_t> bytes, std::string_view layer, size_t bytesMost);

  [[nodiscard]] std::expected<std::span<const uint8_t>, ParseError>
  ReadLayerTables(std::span<const uint8_t> bytes);
  [[nodiscard]] std::expected<void, ParseError> DecodeFeatures(std::span<const uint8_t> layerBytes,
                                                               const AllocationPlan &plan);

  int Extent_ = 4096;
  std::vector<Feature> Features_;
  std::vector<Ring> Rings_;
  std::vector<int32_t> Points_;
  std::vector<uint32_t> Tags_;
  std::vector<std::string> Keys_;
  std::vector<double> Values_;
  std::vector<std::string> ValueStrs_;
  std::vector<bool> ValueIsNum_;
};

}
#endif
