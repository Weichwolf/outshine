#ifndef OUTSHINE_ENGINE_STRUCTURETILEPUBLICATION_H
#define OUTSHINE_ENGINE_STRUCTURETILEPUBLICATION_H

#include "EngineHeld.h"
#include <cassert>
#include <expected>
#include <string>
#include <utility>

namespace outshine {
[[nodiscard]] inline std::expected<void, std::string>
PublishStructureTile(Surrounds &world,
                     Render::SceneRenderer &renderer,
                     const StructureBuildQueue::Landing &landing) {
  assert(landing.Baked);
  const auto published = renderer.PublishedWorld();
  TilePieces pieces = world.Pieces;
  pieces.Into(&renderer);
  const auto &baked = *landing.Baked;
  std::string error;
  if (!pieces.Hands(landing.Tile, baked, landing.AnchorEcef, error, landing.SourceKey)) {
    return std::unexpected(std::move(error));
  }
  world.Pieces = std::move(pieces);
  world.BindSceneResources(renderer);
  return {};
}
}
#endif
