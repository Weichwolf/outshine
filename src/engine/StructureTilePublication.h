#ifndef OUTSHINE_ENGINE_STRUCTURETILEPUBLICATION_H
#define OUTSHINE_ENGINE_STRUCTURETILEPUBLICATION_H

#include "EngineHeld.h"
#include <cassert>
#include <cstdint>
#include <expected>
#include <span>
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

[[nodiscard]] inline std::expected<void, std::string>
StageStructureCell(Surrounds &world,
                   Render::SceneRenderer &renderer,
                   const StructureBuildQueue::Landing &landing) {
  assert(landing.Baked);
  const auto published = renderer.PublishedWorld();
  world.BindSceneResources(renderer);
  const auto &baked = *landing.Baked;
  if (!baked.RequestedCell) {
    return std::unexpected("staged structure product has no cell address");
  }
  std::string error;
  if (!world.Pieces.StageCell(landing.Tile,
                              *baked.RequestedCell,
                              baked,
                              landing.AnchorEcef,
                              error,
                              landing.SourceKey)) {
    return std::unexpected(std::move(error));
  }
  return {};
}

[[nodiscard]] inline std::expected<void, std::string>
ActivateStructureCells(Surrounds &world,
                       Render::SceneRenderer &renderer,
                       uint32_t tile,
                       uint64_t sourceKey,
                       uint64_t occupiedCells,
                       std::span<const TilePieces::CellSelection> selected) {
  const auto published = renderer.PublishedWorld();
  world.BindSceneResources(renderer);
  std::string error;
  if (!world.Pieces.ActivateCells(
          tile, {.Key = sourceKey, .Occupied = occupiedCells}, selected, error)) {
    return std::unexpected(std::move(error));
  }
  return {};
}
}
#endif
