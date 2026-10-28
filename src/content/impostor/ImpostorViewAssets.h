#ifndef OUTSHINE_CONTENT_IMPOSTOR_IMPOSTORVIEWASSETS_H
#define OUTSHINE_CONTENT_IMPOSTOR_IMPOSTORVIEWASSETS_H

#include "AssetCache.h"
#include "ImpostorCards.h"
#include "ImpostorAtlas.h"
#include "Geometry.h"
#include "Vec3.h"
#include "Mat4.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Content {
class ImpostorViewAssets {
public:
  struct Description {
    Vec3 Centre;
    double HalfExtentM = 0;
    uint32_t Pixels = 0;
    std::vector<Vec3> Directions;
    std::optional<Mat4> WorldFromField = std::nullopt;
  };

  struct Selection {
    size_t View = 0;
    uint32_t Mip = 0;
    [[nodiscard]] bool operator==(const Selection &) const = default;
  };

  using Factory = std::function<std::expected<Geometry, AssetCacheError>(Selection, size_t)>;

  [[nodiscard]] static std::expected<ImpostorViewAssets, AssetCacheError>
  Publish(AssetCache &cache, std::string_view provenance, Description description, Box bounds);
  [[nodiscard]] static std::expected<std::optional<ImpostorViewAssets>, AssetCacheError>
  Open(AssetCache &cache, std::string_view provenance);

  [[nodiscard]] const Description &Metadata() const noexcept { return Description_; }

  [[nodiscard]] std::optional<Selection> Select(Vec3 towardEye, double diameterPx) const noexcept;
  [[nodiscard]] std::string Key(Selection selection) const;
  [[nodiscard]] std::expected<void, AssetCacheError>
  Publish(Selection selection, Geometry geometry, size_t bytesMost) const;
  [[nodiscard]] std::expected<std::optional<ImpostorCards>, AssetCacheError>
  Load(Selection selection, size_t bytesMost) const;
  // Run on the existing content worker; a validated hit never invokes the factory.
  [[nodiscard]] std::expected<ImpostorCards, AssetCacheError>
  Resolve(Selection selection, size_t bytesMost, const Factory &factory) const;

  struct ViewIndex {
    size_t Value = 0;
  };

  using FieldFactory =
      std::function<std::expected<ImpostorAtlas, AssetCacheError>(ViewIndex, size_t)>;
  [[nodiscard]] std::string FieldKey(ViewIndex view) const;
  [[nodiscard]] std::expected<void, AssetCacheError>
  PublishField(ViewIndex view, const ImpostorAtlas &field, size_t bytesMost) const;
  [[nodiscard]] std::expected<std::optional<ImpostorAtlas>, AssetCacheError>
  LoadField(ViewIndex view, size_t bytesMost) const;
  [[nodiscard]] std::expected<ImpostorAtlas, AssetCacheError>
  ResolveField(ViewIndex view, size_t bytesMost, const FieldFactory &factory) const;

private:
  ImpostorViewAssets(AssetCache &cache, AssetRecord record, Description description);
  [[nodiscard]] uint32_t Pixels(Selection selection) const noexcept;
  [[nodiscard]] bool Matches(const ImpostorCards &cards, Selection selection) const;
  [[nodiscard]] bool MatchesField(const ImpostorAtlas &field, ViewIndex view) const noexcept;
  [[nodiscard]] std::expected<std::vector<uint8_t>, AssetCacheError>
  StoreField(ViewIndex view, const ImpostorAtlas &field, size_t bytesMost) const;
  AssetCache &Cache_;
  AssetRecord Record_;
  Description Description_;
};
}
#endif
