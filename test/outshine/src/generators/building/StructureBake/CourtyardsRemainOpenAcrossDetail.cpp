#include "BuildingMesh.h"
#include "BuildingStampJob.h"
#include "Check.h"
#include "Geodesy.h"
#include "StructureBake.h"
#include "math/Units.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

namespace {
using Point = std::array<double, 2>;

double Area(std::span<const Point> ring) {
  double area = 0;
  for (size_t at = 0; at < ring.size(); ++at) {
    const auto &a = ring[at];
    const auto &b = ring[(at + 1) % ring.size()];
    area += a[0] * b[1] - a[1] * b[0];
  }
  return std::abs(area) * 0.5;
}

double Overlap(std::vector<Point> polygon, double low, double high) {
  for (size_t axis = 0; axis < 2; ++axis) {
    for (const bool lower : {true, false}) {
      std::vector<Point> clipped;
      const double boundary = lower ? low : high;
      for (size_t at = 0; at < polygon.size(); ++at) {
        const Point a = polygon[at], b = polygon[(at + 1) % polygon.size()];
        const bool inA = lower ? a[axis] >= boundary : a[axis] <= boundary;
        const bool inB = lower ? b[axis] >= boundary : b[axis] <= boundary;
        if (inA) { clipped.push_back(a); }
        if (inA != inB) {
          const double t = (boundary - a[axis]) / (b[axis] - a[axis]);
          clipped.push_back({a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])});
        }
      }
      polygon = std::move(clipped);
    }
  }
  return Area(polygon);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  const std::array<Point, 12> points{{{0, 0},
                                      {40, 0},
                                      {40, 40},
                                      {0, 40},
                                      {10, 10},
                                      {10, 30},
                                      {30, 30},
                                      {30, 10},
                                      {2, 2},
                                      {2, 6},
                                      {6, 6},
                                      {6, 2}}};
  for (const auto &point : points) {
    raw.LatLon.push_back(point[1] / kMPerDegLat);
    raw.LatLon.push_back(point[0] / kMPerDegLon);
  }
  const auto cell =
      StructureCellOf({.MinLonDeg = 0, .MinLatDeg = 0, .MaxLonDeg = 0.008, .MaxLatDeg = 0.008},
                      std::span(raw.LatLon).first(8));
  CHECK(cell.has_value(), "courtyard lies in one source cell");
  if (!cell) { return Report(); }
  raw.Holes = {{.First = 4, .Count = 4, .Exterior = false},
               {.First = 8, .Count = 4, .Exterior = false}};
  raw.Structures.push_back(
      {.PointCount = 4, .HoleCount = 2, .Cell = *cell, .HeightM = 12, .Pitched = 0});
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 0, .LatitudeDeg = 0, .HeightM = 0}, raw.AnchorEcef);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0);
  const auto heights = Ground::HeightField::Of(0, {block});
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  for (const int pitched : {0, 1}) {
    raw.Structures[0].Pitched = pitched;
    for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
      raw.RequestedDetail = detail;
      BakedTile product;
      const auto built = BakeStructures(raw, *heights, mesher, *scratch, product);
      CHECK(built.has_value(),
            "perforated building bakes with flat and pitched roofs at every detail");
      if (!built) { continue; }
      CHECK(product.Prints.size() == 1 && product.Prints[0].HoleCount == 2 && product.Lumped == 0,
            "source courtyard ranges survive and prevent solid aggregation");
      double roofArea = 0, coveredCourt = 0;
      const auto inspect = [&](const auto &vertices, const auto &indices, bool roof) {
        for (size_t at = 0; at + 2 < indices.size(); at += 3) {
          std::vector<Point> triangle;
          for (size_t corner = 0; corner < 3; ++corner) {
            const auto &vertex = vertices[indices[at + corner]];
            triangle.push_back({vertex.pos[1], vertex.pos[2]});
          }
          if (roof) { roofArea += Area(triangle); }
          coveredCourt += Overlap(triangle, 10, 30) + Overlap(triangle, 2, 6);
        }
      };
      inspect(product.Built.RoofCorners, product.Built.RoofRun, true);
      inspect(product.Built.WallCorners, product.Built.WallRun, false);
      CHECK(std::abs(roofArea - (40 * 40 - 20 * 20 - 4 * 4)) < 0.02,
            "projected roof area equals outer rectangle minus both courtyards");
      CHECK(coveredCourt < 0.02,
            "independent polygon clipping finds no roof or floor over either courtyard");
      for (const size_t budget : {size_t{1}, size_t{64}}) {
        BuildingStampJob job(TangentFrame::At({.LongitudeDeg = 0, .LatitudeDeg = 0}), 1);
        bool done = false;
        for (size_t step = 0; step < 100 && !done; ++step) {
          const auto next = job.Advance({.Footprints = product.Prints,
                                         .Points = raw.LatLon,
                                         .Rings = raw.Holes,
                                         .VectorGeneration = 1,
                                         .UnitsMost = budget});
          CHECK(next.has_value(), "bounded terrain stamp retains courtyard source ranges");
          if (!next) { break; }
          done = *next;
        }
        const auto stamps = std::move(job).Take();
        CHECK(done && stamps && stamps->size() == 1 &&
                  stamps->front().HoleRingsEastNorthM.size() == 2 &&
                  stamps->front().HoleRingsEastNorthM[0].size() == 8,
              "terrain receives both holes even when interrupted after every work unit");
      }
    }
  }
  auto outside = raw;
  for (size_t at = 8; at < 16; ++at) { outside.LatLon[at] += 0.01; }
  BakedTile outsideProduct;
  CHECK(!BakeStructures(outside, *heights, mesher, *scratch, outsideProduct),
        "a courtyard outside its exterior fails instead of silently omitting the building");
  raw.Holes[0].First = 1000;
  BakedTile invalid;
  CHECK(!BakeStructures(raw, *heights, mesher, *scratch, invalid),
        "invalid inner coordinate ownership cannot publish a building");
  return Report();
}
