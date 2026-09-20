#include "ImpostorPreparation.h"

#include "CrownBuild.h"
#include "ImpostorBaker.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {

std::string ImpostorAtlasProvenance(std::string_view species, Content::ImpostorAtlasShape shape) {
  return std::string(kCrownBuildIdentity) + "/" + std::to_string(shape.Pixels) + "/" +
         std::to_string(shape.Views) + "/" + std::string(species);
}

std::optional<Content::ImpostorAtlas> BakeImpostorAtlas(const Generators::TreePrototype &tree,
                                                        Content::ImpostorAtlasShape shape,
                                                        std::string &error) {
  auto geometry = tree.InstancedGeometryAt(0);
  if (!geometry) {
    error = "impostor capture requires native tree geometry";
    return std::nullopt;
  }
  Render::ImpostorCapture capture;
  capture.LeastM = geometry->LeastM;
  capture.MostM = geometry->MostM;
  capture.Sources.push_back(
      {.Mesh = std::move(geometry->Bark), .Instances = std::vector<Mat4>{Mat4{}}});
  if (!geometry->Placements.empty() && geometry->Leaf.parts() > 0) {
    capture.Sources.push_back(
        {.Mesh = std::move(geometry->Leaf), .Instances = std::move(geometry->Placements)});
  }
  return Render::ImpostorBaker::Bake(std::move(capture), shape, error);
}

}
