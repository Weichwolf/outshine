#include "Cooked.h"

#include <vector>

namespace outshine {

bool Cook(const Geometry &what, int part, const ClusterDagOpts &how, CookedPart &into,
          std::string &error) {
  const std::span<const float> places = what.PositionsOf(part);
  const std::span<const uint32_t> triangles = what.TrianglesOf(part);
  if (places.empty() || triangles.size() < 3 || triangles.size() % 3 != 0) {
    error = "part " + std::to_string(part) + " carries " + std::to_string(places.size() / 3) +
            " vertex/vertices and " + std::to_string(triangles.size()) +
            " index/indices, which is not a mesh a cooker can cluster";
    return false;
  }

  const std::span<const float> normals = what.NormalsOf(part);
  const std::span<const float> texture = what.TextureOf(part, 0);
  const std::span<const float> tangents = what.TangentsOf(part);
  const std::span<const float> colours = what.ColoursOf(part);
  const size_t vertices = places.size() / 3;

  into.HasNormals = normals.size() == vertices * 3;
  into.HasTexture = texture.size() == vertices * 2;
  into.HasTangents = tangents.size() == vertices * 4;
  into.HasColours = colours.size() == vertices * 4;

  const int stride = 3 + (into.HasNormals ? 3 : 0) + (into.HasTexture ? 2 : 0) +
                     (into.HasTangents ? 4 : 0) + (into.HasColours ? 4 : 0);

  std::vector<float> soup;
  soup.reserve(triangles.size() * (size_t)stride);
  for (const uint32_t corner : triangles) {
    if ((size_t)corner >= vertices) {
      error = "part " + std::to_string(part) + " names vertex " + std::to_string(corner) +
              " over " + std::to_string(vertices) + " -- a cooker will not guess what it meant";
      return false;
    }
    soup.insert(soup.end(), places.begin() + (long)corner * 3, places.begin() + (long)corner * 3 + 3);
    if (into.HasNormals) {
      soup.insert(soup.end(), normals.begin() + (long)corner * 3,
                  normals.begin() + (long)corner * 3 + 3);
    }
    if (into.HasTexture) {
      soup.insert(soup.end(), texture.begin() + (long)corner * 2,
                  texture.begin() + (long)corner * 2 + 2);
    }
    if (into.HasTangents) {
      soup.insert(soup.end(), tangents.begin() + (long)corner * 4,
                  tangents.begin() + (long)corner * 4 + 4);
    }
    if (into.HasColours) {
      soup.insert(soup.end(), colours.begin() + (long)corner * 4,
                  colours.begin() + (long)corner * 4 + 4);
    }
  }

  if (!ClusterDagBuild(soup.data(), (uint32_t)(soup.size() / (size_t)stride), stride, how,
                       &into.Dag)) {
    error = "the cooker refused part " + std::to_string(part) + " at stride " +
            std::to_string(stride);
    return false;
  }
  return true;
}

}
