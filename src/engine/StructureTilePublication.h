#ifndef OUTSHINE_ENGINE_STRUCTURETILEPUBLICATION_H
#define OUTSHINE_ENGINE_STRUCTURETILEPUBLICATION_H

#include "EngineHeld.h"
#include "WorldCandidate.h"
#include <cassert>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace outshine {
[[nodiscard]] inline std::expected<void, std::string>
PublishStructureTile(Surrounds &world,
                     Render::SceneRenderer &renderer,
                     std::unique_ptr<Core::RuntimeScene> &scene,
                     const StructureBuildQueue::Landing &landing,
                     const Ui::Font *font) {
  assert(scene && landing.Baked);
  Core::WorldCandidate candidate(renderer);
  if (auto prepared = candidate.Prepare(*scene, font); !prepared) { return prepared; }
  TilePieces pieces = world.Pieces;
  pieces.Into(&renderer);
  const auto &baked = *landing.Baked;
  std::string error;
  if (!pieces.Hands(landing.Tile, baked, landing.AnchorEcef, error)) {
    return std::unexpected(std::move(error));
  }
  if (auto published = candidate.Publish(scene); !published) { return published; }
  static_assert(std::is_nothrow_move_assignable_v<TilePieces>);
  world.Pieces = std::move(pieces);
  world.BindSceneResources(renderer);
  return {};
}

[[nodiscard]] inline std::expected<void, std::string>
PublishStructureTiles(Surrounds &world,
                      Render::SceneRenderer &renderer,
                      std::unique_ptr<Core::RuntimeScene> &scene,
                      std::span<const StructureBuildQueue::Landing> landings,
                      const Ui::Font *font) {
  assert(scene);
  Core::WorldCandidate candidate(renderer);
  if (auto prepared = candidate.Prepare(*scene, font); !prepared) { return prepared; }
  TilePieces pieces = world.Pieces;
  pieces.Into(&renderer);
  std::string error;
  for (const StructureBuildQueue::Landing &landing : landings) {
    assert(landing.Baked);
    if (!pieces.Hands(landing.Tile, *landing.Baked, landing.AnchorEcef, error)) {
      return std::unexpected(std::move(error));
    }
  }
  if (auto published = candidate.Publish(scene); !published) { return published; }
  static_assert(std::is_nothrow_move_assignable_v<TilePieces>);
  world.Pieces = std::move(pieces);
  world.BindSceneResources(renderer);
  return {};
}
}
#endif
