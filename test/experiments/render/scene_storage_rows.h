#ifndef OUTSHINE_EXPERIMENT_SCENE_STORAGE_ROWS_H
#define OUTSHINE_EXPERIMENT_SCENE_STORAGE_ROWS_H

#include "Shape.h"
#include "scene/Geometry.h"

#include <cstddef>
#include <cstdio>
#include <print>
#include <string_view>

namespace outshine::Core::StorageProbe {

inline void Report(const char *phase,
                   const void *owner,
                   const Geometry &held,
                   const Geometry &driven,
                   const Render::ShapeStore &shape) {
  const auto bytes = [](const auto &values) { return values.capacity() * sizeof(values[0]); };
  const size_t total = bytes(shape.Parts) + bytes(shape.PositionsM) + bytes(shape.Normals) +
                       bytes(shape.Tangents) + bytes(shape.Uv) + bytes(shape.Uv1) +
                       bytes(shape.Colours) + bytes(shape.Indices) + bytes(shape.Surfaces) +
                       bytes(shape.Lamps) + bytes(shape.Clusters) + bytes(shape.ClusterSpheres);
  if (held.storageBytes() + driven.storageBytes() + total < 1048576) { return; }
  std::println(stderr,
               "OWNER\t{}\t{}\theld\t{}\tdriven\t{}\tshape\t{}",
               phase,
               owner,
               held.storageBytes(),
               driven.storageBytes(),
               total);
  const auto row = [&](const char *name, const auto &values) {
    std::println(stderr,
                 "ARRAY\t{}\t{}\t{}\t{}\t{}\t{}",
                 phase,
                 owner,
                 name,
                 values.size() * sizeof(values[0]),
                 bytes(values),
                 static_cast<const void *>(values.data()));
  };
  row("parts", shape.Parts);
  row("positions", shape.PositionsM);
  row("normals", shape.Normals);
  row("tangents", shape.Tangents);
  row("uv", shape.Uv);
  row("uv1", shape.Uv1);
  row("colours", shape.Colours);
  row("indices", shape.Indices);
  row("surfaces", shape.Surfaces);
  row("lamps", shape.Lamps);
  row("clusters", shape.Clusters);
  row("spheres", shape.ClusterSpheres);
  if (std::string_view(phase) != "geometry-held") { return; }
  for (int part = 0; part < held.parts(); ++part) {
    const auto &placement = held.placementOf(part);
    std::print(stderr, "PLACEMENT\t{}\t{}\t{}", owner, part, placement == Mat4{} ? 1 : 0);
    for (size_t element = 0; element < 16; ++element) {
      std::print(stderr, "\t{:.17g}", placement[element]);
    }
    std::println(stderr, "");
    std::println(stderr,
                 "PART\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
                 owner,
                 part,
                 held.positionsOf(part).size(),
                 held.trianglesOf(part).size(),
                 held.normalsOf(part).size(),
                 held.tangentsOf(part).size(),
                 held.textureOf(part, Geometry::UvSet::Uv0).size(),
                 held.textureOf(part, Geometry::UvSet::Uv1).size(),
                 held.coloursOf(part).size());
  }
}

}
#endif
