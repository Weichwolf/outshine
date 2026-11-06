#include "RoadValueFields.h"
#include <span>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace outshine::Generators::RoadValueFields {
bool Body(BinaryValueWriter &out, const HeightSolid &body) {
  return WriteList(out, std::span(body.Points), [&](const auto &point) { return out(point); }) &&
         WriteList(out, std::span(body.Faces), [&](const auto &face) { return out(face); }) &&
         out.Array(std::span(body.WallParents)) &&
         WriteList(out, std::span(body.WallKinds), [&](auto kind) { return out(kind); }) &&
         out(static_cast<uint64_t>(body.SurfaceFaces), body.VolumeM3);
}

bool ValidBody(const HeightSolid &body) {
  return !body.Points.empty() && !body.Faces.empty() && body.SurfaceFaces > 0 &&
         body.SurfaceFaces <= body.Faces.size() && std::isfinite(body.VolumeM3) &&
         body.VolumeM3 >= 0 && body.WallParents.size() == body.WallKinds.size() &&
         std::ranges::all_of(body.Points,
                             [](const auto &point) {
                               return std::ranges::all_of(
                                   point, [](double v) { return std::isfinite(v); });
                             }) &&
         std::ranges::all_of(body.Faces,
                             [&](const auto &face) {
                               return face[0] != face[1] && face[1] != face[2] &&
                                      face[2] != face[0] &&
                                      std::ranges::all_of(face, [&](uint32_t index) {
                                        return index < body.Points.size();
                                      });
                             }) &&
         std::ranges::all_of(body.WallParents,
                             [&](uint32_t parent) { return parent < body.SurfaceFaces; }) &&
         std::ranges::all_of(body.WallKinds, [](auto kind) {
           return kind == HeightSolidWallKind::Boundary ||
                  kind == HeightSolidWallKind::Discontinuity;
         });
}

}
