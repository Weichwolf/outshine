#include "PlanHierarchy.h"
#include "PolygonPrism.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ios>
#include <ratio>
#include <string>
#include <fstream>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <vector>

namespace {
using namespace outshine;

struct Source {
  uint64_t First = 0, Count = 0;
  double Bottom = 0, Top = 0;
  Vec3f Colour;
};

struct Scene {
  std::vector<Vec2> Points;
  std::vector<std::span<const Vec2>> Rings;
  std::vector<PolygonPrism> Prisms;
  std::vector<Vec3f> Colours;
  Vec3 Eye;
  std::array<Vec3, 3> Basis{};
  uint32_t Width = 0, Height = 0;
  double Focal = 0;
};

template <class T> bool Read(std::ifstream &input, T &value) {
  return static_cast<bool>(input.read(reinterpret_cast<char *>(&value), sizeof(T)));
}

bool ReadPoints(std::ifstream &input, uint64_t count, Scene &scene) {
  scene.Points.resize(count);
  for (Vec2 &point : scene.Points) {
    for (double &value : point) {
      if (!Read(input, value) || !std::isfinite(value)) { return false; }
    }
  }
  return true;
}

bool ReadRings(std::ifstream &input, uint64_t count, Scene &scene) {
  scene.Rings.reserve(count);
  for (uint64_t at = 0; at < count; ++at) {
    uint64_t first = 0, length = 0;
    if (!Read(input, first) || !Read(input, length) || first > scene.Points.size() ||
        length > scene.Points.size() - first || length < 3) {
      return false;
    }
    scene.Rings.push_back(std::span(scene.Points).subspan(first, length));
  }
  return true;
}

bool ReadPrisms(std::ifstream &input, uint64_t count, Scene &scene) {
  scene.Prisms.reserve(count);
  scene.Colours.reserve(count);
  for (uint64_t at = 0; at < count; ++at) {
    Source source;
    if (!Read(input, source.First) || !Read(input, source.Count) || !Read(input, source.Bottom) ||
        !Read(input, source.Top)) {
      return false;
    }
    for (float &value : source.Colour) {
      if (!Read(input, value) || !std::isfinite(value)) { return false; }
    }
    if (source.First > scene.Rings.size() || source.Count == 0 ||
        source.Count > scene.Rings.size() - source.First || !std::isfinite(source.Bottom) ||
        !std::isfinite(source.Top) || source.Bottom >= source.Top) {
      return false;
    }
    scene.Prisms.push_back(
        {.Ring = scene.Rings[source.First],
         .Holes = std::span(scene.Rings).subspan(source.First + 1, source.Count - 1),
         .Bottom = source.Bottom,
         .Top = source.Top});
    scene.Colours.push_back(source.Colour);
  }
  return true;
}

bool Load(const char *path, Scene &scene) {
  std::ifstream input(path, std::ios::binary);
  uint64_t plans = 0, rings = 0, points = 0;
  constexpr uint64_t limit = 16'000'000;
  if (!Read(input, plans) || !Read(input, rings) || !Read(input, points) || plans > limit ||
      rings > limit || points > limit || !Read(input, scene.Width) || !Read(input, scene.Height)) {
    return false;
  }
  for (double &value : scene.Eye) {
    if (!Read(input, value)) { return false; }
  }
  for (Vec3 &axis : scene.Basis) {
    for (double &value : axis) {
      if (!Read(input, value)) { return false; }
    }
  }
  if (!Read(input, scene.Focal) || !std::isfinite(scene.Focal) || scene.Focal <= 0.0 ||
      scene.Width == 0 || scene.Width > 4096 || scene.Height == 0 || scene.Height > 4096) {
    return false;
  }
  return ReadPoints(input, points, scene) && ReadRings(input, rings, scene) &&
         ReadPrisms(input, plans, scene) && input.peek() == std::char_traits<char>::eof();
}

std::array<uint8_t, 3> ColourOf(const Scene &scene,
                                const std::optional<PlanHierarchy::RayHit> &hit,
                                const std::optional<PolygonPrism::Hit> &surface) {
  constexpr std::array<double, 3> background{{0.23, 0.36, 0.5}};
  constexpr std::array<double, 3> roof{{0.8, 0.76, 0.72}};
  constexpr double ambient = 0.38, direct = 0.62;
  const Vec3 light = Vec3{{1, -1, 2}} * (1.0 / std::sqrt(6.0));
  std::array<uint8_t, 3> rgb{};
  double lighting = 0.0;
  if (surface) { lighting = ambient + direct * std::max(Dot(surface->Normal, light), 0.0); }
  for (size_t axis = 0; axis < 3; ++axis) {
    double value = background[axis];
    if (hit && surface) {
      value = scene.Colours[hit->Source][axis] * lighting;
      if (surface->Face == 1) { value *= roof[axis]; }
    }
    rgb[axis] = static_cast<uint8_t>(
        std::lround(std::clamp(value, 0.0, 1.0) * std::numeric_limits<uint8_t>::max()));
  }
  return rgb;
}

struct Capture {
  std::vector<uint8_t> Rgb;
  size_t PrimitiveCalls = 0, CoveredPixels = 0;
};

Capture Trace(const Scene &scene, const PlanHierarchy &index) {
  Capture capture;
  capture.Rgb.resize(static_cast<size_t>(scene.Width) * scene.Height * 3);
  for (uint32_t y = 0; y < scene.Height; ++y) {
    for (uint32_t x = 0; x < scene.Width; ++x) {
      const Vec3 ray = scene.Basis[0] * ((x + 0.5 - scene.Width * 0.5) / scene.Focal) +
                       scene.Basis[1] * ((scene.Height * 0.5 - y - 0.5) / scene.Focal) +
                       scene.Basis[2];
      std::optional<PolygonPrism::Hit> surface;
      const auto hit = index.TraceClosest(
          scene.Eye,
          ray,
          0.1,
          std::numeric_limits<double>::infinity(),
          [&](uint32_t source, double maximum) -> std::optional<double> {
            ++capture.PrimitiveCalls;
            const auto candidate = scene.Prisms[source].Trace(scene.Eye, ray, 0.1, maximum);
            if (!candidate || candidate->Face == 0 || Dot(candidate->Normal, ray) >= 0.0) {
              return std::nullopt;
            }
            surface = candidate;
            return candidate->Along;
          });
      if (hit) { ++capture.CoveredPixels; }
      const auto rgb = ColourOf(scene, hit, surface);
      for (size_t axis = 0; axis < 3; ++axis) {
        capture.Rgb[(static_cast<size_t>(y) * scene.Width + x) * 3 + axis] = rgb[axis];
      }
    }
  }
  return capture;
}

double Elapsed(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
      .count();
}

bool Save(const char *path, const Scene &scene, const Capture &capture) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << scene.Width << ' ' << scene.Height << "\n255\n";
  output.write(reinterpret_cast<const char *>(capture.Rgb.data()),
               static_cast<std::streamsize>(capture.Rgb.size()));
  output.close();
  return static_cast<bool>(output);
}
}

int main(int argc, char **argv) {
  using namespace outshine::Test;
  Scene scene;
  if (argc == 1) {
    scene.Points = {{{-10, -10}}, {{10, -10}}, {{10, 10}}, {{-10, 10}}};
    scene.Rings.push_back(scene.Points);
    scene.Prisms.push_back({.Ring = scene.Rings[0], .Holes = {}, .Bottom = 0, .Top = 10});
    scene.Colours.push_back({{1, 1, 1}});
    scene.Eye = {{0, 0, 20}};
    scene.Basis = {{{{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, -1}}}};
    scene.Width = scene.Height = 16;
    scene.Focal = 16;
  } else if (argc != 3 || !Load(argv[1], scene)) {
    std::println(stderr, "expected a valid source-prism input and a PPM output path");
    return 1;
  }
  std::vector<Box> bounds;
  bounds.reserve(scene.Prisms.size());
  for (const auto &prism : scene.Prisms) { bounds.push_back(prism.Bounds()); }
  const auto began = std::chrono::steady_clock::now();
  const auto index = PlanHierarchy::Build(bounds);
  const double buildMs = Elapsed(began);
  CHECK(index, "source-prism hierarchy builds with no render vertices");
  if (!index) { return Report(); }
  const auto traceStart = std::chrono::steady_clock::now();
  const auto capture = Trace(scene, *index);
  const double traceMs = Elapsed(traceStart);
  if (argc == 1) {
    CHECK(capture.CoveredPixels == 256 && capture.PrimitiveCalls == 256,
          "every sample queries exactly one source without triangulation");
    return Report();
  }
  if (!Save(argv[2], scene, capture)) { return 1; }
  std::println("plans={} nodes={} rays={} primitive_calls={} covered={} build_ms={:.3f} "
               "capture_ms={:.3f} source_triangles=0",
               scene.Prisms.size(),
               index->Nodes().size(),
               static_cast<size_t>(scene.Width) * scene.Height,
               capture.PrimitiveCalls,
               capture.CoveredPixels,
               buildMs,
               traceMs);
  return Report();
}
