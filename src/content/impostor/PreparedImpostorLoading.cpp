#include "PreparedImpostorAssets.h"
#include "Sha256.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Content {
std::optional<ImpostorCards> PreparedImpostorAssets::BuildCards(const ImpostorAtlas &atlas,
                                                                std::string &error) {
  const auto began = std::chrono::steady_clock::now();
  auto cards = Prepare_(atlas, error);
  ++Preparations_;
  PreparationMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return cards;
}

std::optional<CachedAsset> PreparedImpostorAssets::LoadBytes(std::string_view key,
                                                             std::string &error) {
  const std::scoped_lock lock(Lock_);
  if (!Opens(error) || MostBytes_ == 0) { return std::nullopt; }
  auto asset = Cache_->Load(key, MostBytes_);
  if (!asset) {
    error = "native impostor package could not be loaded within its budget";
    return std::nullopt;
  }
  if (*asset) { ReadBytes_ += (**asset).Bytes().size(); }
  return std::move(*asset);
}

PreparedImpostorAssets::Loaded PreparedImpostorAssets::Load(std::string provenance) {
  Loaded result{.Provenance = std::move(provenance), .Atlas = {}, .Cards = {}, .Error = {}};
  if (Prepare_ != nullptr) {
    if (const auto ready =
            LoadBytes(Sha256Hex("impostor-cards-1/" + result.Provenance), result.Error)) {
      result.Cards = ImpostorCards::Decode(ready->Bytes(), result.Provenance, MostBytes_);
      if (result.Cards) {
        ++Hits_;
        return result;
      }
    }
  }
  const auto asset = LoadBytes(Sha256Hex(result.Provenance), result.Error);
  if (asset) {
    auto atlas = ImpostorAtlas::Decode(asset->Bytes(), result.Provenance, result.Error);
    if (atlas && Prepare_ != nullptr) {
      result.Error.clear();
      result.Cards = BuildCards(*atlas, result.Error);
      if (result.Cards && !PublishCards(*result.Cards, result.Provenance, result.Error)) {
        result.Cards.reset();
      }
    } else {
      result.Atlas = std::move(atlas);
    }
  }
  if (result.Atlas || result.Cards) {
    ++Hits_;
  } else {
    ++Misses_;
    if (result.Error.empty()) { result.Error = "native impostor is absent or unreadable"; }
  }
  return result;
}
}
