#include "GroundRegionRequest.h"
#include "PreparedRoadPages.h"
#include "GroundRegionAsset.h"
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators {
namespace {
using RegionRequest = GroundRegionRequest;
using Regions = Osm::PreparedGroundRegions;

struct LookupState {
  std::shared_ptr<Regions> Assets;
  std::string Request;
  RegionRequest::KeyFactory KeyFactory;
  bool CheckedRequest = false, CheckedContent = false;
  std::string Key;
  std::optional<Regions::Loaded> Held;

  RegionRequest::Attempt Advance() {
    if (!CheckedRequest) {
      auto loaded = Assets->LoadRequest(Request);
      if (!loaded) { return std::unexpected(std::move(loaded.error())); }
      CheckedRequest = true;
      if (*loaded) {
        auto key = (**loaded).Key;
        return RegionRequest::Completed{.Key = std::move(key),
                                        .Loaded = std::move(*loaded),
                                        .RequestKey = Request,
                                        .RequestHit = true,
                                        .SurfacePages = nullptr};
      }
    }
    if (Key.empty()) {
      if (!KeyFactory) {
        return RegionRequest::Completed{
            .Key = {}, .Loaded = std::nullopt, .RequestKey = Request, .SurfacePages = nullptr};
      }
      auto key = KeyFactory();
      if (!key) { return std::unexpected(std::move(key.error())); }
      if (key->empty()) {
        return std::unexpected(std::string("ground region key exceeds its encoding limit"));
      }
      Key = std::move(*key);
      KeyFactory = {};
    }
    if (!CheckedContent) {
      auto loaded = Assets->Load(Key);
      if (!loaded) { return std::unexpected(std::move(loaded.error())); }
      Held = std::move(*loaded);
      CheckedContent = true;
    }
    if (Held) {
      const auto bound = Assets->BindRequest(Key, Request);
      if (!bound) { return std::unexpected(bound.error()); }
    }
    return RegionRequest::Completed{
        .Key = Key, .Loaded = std::move(Held), .RequestKey = Request, .SurfacePages = nullptr};
  }
};

struct StoreState {
  RegionRequest::StoreInputs Inputs;
  std::optional<Regions::StoreWork> Ground;
  std::optional<PreparedRoadPages::ManifestWrite> Manifest;
  Box Bounds;
  bool BoundsReady = false, ManifestDone = false;

  std::expected<void, RegionRequest::Failure> PrepareManifest(const std::stop_token &stop) {
    if (!Inputs.Manifest && !Manifest) { return {}; }
    if (!Inputs.Pages) {
      return std::unexpected(std::string("road manifest has no native cache service"));
    }
    if (!Manifest) {
      auto prepared = Inputs.Pages->PrepareManifest(*Inputs.Manifest);
      if (!prepared) { return std::unexpected(std::move(prepared.error())); }
      if (!Inputs.Region || prepared->Key() != Inputs.Region->SurfaceManifest) {
        return std::unexpected(
            std::string("stored road manifest differs from its region reference"));
      }
      Manifest = std::move(*prepared);
      Inputs.Manifest.reset();
    }
    const auto stored = Inputs.Pages->StoreManifest(*Manifest, stop);
    if (!stored) { return std::unexpected(stored.error()); }
    Manifest.reset();
    ManifestDone = true;
    return {};
  }

  RegionRequest::Attempt Advance(const std::stop_token &stop) {
    if (!Inputs.Assets) {
      return std::unexpected(std::string("ground region cache service is missing"));
    }
    if (!BoundsReady) {
      if (!Inputs.Region || !Inputs.Water) {
        return std::unexpected(std::string("ground publication inputs are missing"));
      }
      Bounds = GroundRegionBoundsEcef(*Inputs.Region);
      if (Inputs.Manifest) { Bounds.Cover(RoadPageManifestBounds(*Inputs.Manifest)); }
      BoundsReady = true;
    }
    if (!ManifestDone) {
      const auto stored = PrepareManifest(stop);
      if (!stored) { return std::unexpected(stored.error()); }
      ManifestDone = true;
    }
    if (!Ground) {
      if (!Inputs.Region || !Inputs.Water) {
        return std::unexpected(std::string("ground publication inputs were released"));
      }
      auto prepared = Inputs.Assets->PrepareStore(
          Inputs.Key, Bounds, *Inputs.Region, *Inputs.Water, Inputs.RequestKey);
      if (!prepared) { return std::unexpected(std::move(prepared.error())); }
      Ground = std::move(*prepared);
      Inputs.Region.reset();
      Inputs.Water.reset();
    }
    auto loaded = Inputs.Assets->Store(*Ground, stop);
    if (!loaded) { return std::unexpected(std::move(loaded.error())); }
    Ground.reset();
    return RegionRequest::Completed{.Key = Inputs.Key,
                                    .Loaded = std::move(*loaded),
                                    .RequestKey = Inputs.RequestKey,
                                    .SurfacePages = nullptr};
  }
};
}

GroundRegionRequest::Factory
GroundRegionRequest::Lookup(std::shared_ptr<Osm::PreparedGroundRegions> assets,
                            std::string request,
                            KeyFactory key,
                            bool checked) {
  auto state = std::make_shared<LookupState>();
  state->Assets = std::move(assets);
  state->Request = std::move(request);
  state->KeyFactory = std::move(key);
  state->CheckedRequest = checked;
  return [state = std::move(state)](const std::stop_token &stop) -> Attempt {
    if (stop.stop_requested()) {
      return std::unexpected(std::string("ground region lookup canceled"));
    }
    if (!state->Assets || state->Request.empty()) {
      return std::unexpected(std::string("ground request key or service is missing"));
    }
    return state->Advance();
  };
}

GroundRegionRequest::Factory GroundRegionRequest::PrepareStore(StoreInputs inputs) {
  auto state = std::make_shared<StoreState>();
  state->Inputs = std::move(inputs);
  return [state = std::move(state)](const std::stop_token &stop) -> Attempt {
    if (stop.stop_requested()) {
      return std::unexpected(std::string("ground region publication canceled"));
    }
    return state->Advance(stop);
  };
}
}
