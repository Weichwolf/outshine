#include "CanonicalGroundSourceFixture.h"
#include "CanonicalGroundTerrainFixture.h"
#include "AssetCache.h"
#include "Check.h"
#include "EarthworkDeformation.h"
#include "Geocentric.h"
#include "GroundPortBodies.h"
#include "GroundPortCore.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include <array>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <limits>
#include <numeric>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

bool Contains(const Box &bounds, Vec3 point) {
  for (size_t axis = 0; axis < 3; ++axis) {
    if (point[axis] < bounds.Min[axis] || point[axis] > bounds.Max[axis]) { return false; }
  }
  return true;
}

bool Spawn(const std::string &executable,
           const std::string &path,
           const std::string &key,
           const Box &bounds,
           Vec3 point) {
  std::vector<std::string> values{executable, "--offline-bounds", path, key};
  values.reserve(13);
  for (size_t axis = 0; axis < 3; ++axis) { values.push_back(std::format("{:.17g}", point[axis])); }
  for (size_t axis = 0; axis < 3; ++axis) {
    values.push_back(std::format("{:.17g}", bounds.Min[axis]));
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    values.push_back(std::format("{:.17g}", bounds.Max[axis]));
  }
  std::vector<char *> arguments;
  arguments.reserve(values.size() + 1);
  for (auto &value : values) { arguments.push_back(value.data()); }
  arguments.push_back(nullptr);
#ifdef __APPLE__
  char *const *const environment = *_NSGetEnviron();
#else
  char *const *const environment = environ;
#endif
  pid_t child = 0;
  if (posix_spawn(&child, executable.c_str(), nullptr, nullptr, arguments.data(), environment) !=
      0) {
    return false;
  }
  int status = 0;
  return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int Offline(char **arguments) {
  std::array<double, 9> coordinates{};
  for (size_t at = 0; at < coordinates.size(); ++at) {
    const std::string_view value(arguments[4 + at]);
    const auto result = std::from_chars(value.data(), value.data() + value.size(), coordinates[at]);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) { return 2; }
  }
  const Vec3 point{{coordinates[0], coordinates[1], coordinates[2]}};
  const Box expected{.Min = {{coordinates[3], coordinates[4], coordinates[5]}},
                     .Max = {{coordinates[6], coordinates[7], coordinates[8]}}};
  auto cache = AssetCache::Open(arguments[2]);
  CHECK(cache.has_value(),
        "a genuinely fresh process opens only the existing native spatial asset index");
  if (!cache) { return Report(); }
  const auto selected = (*cache)->Select({.Bounds = {.Min = point, .Max = point},
                                          .Kind = "ground-body-bounds-probe",
                                          .Level = {},
                                          .Radius = {},
                                          .Planes = {}});
  CHECK(selected && selected->size() == 1 && selected->front().Key == arguments[3] &&
            selected->front().Bounds == expected,
        "offline spatial discovery retains the exact actual body/contact bounds and finds the "
        "distant owner");
  const auto receipt = (*cache)->Load(arguments[3], 32);
  CHECK(receipt && *receipt && (**receipt).Bytes().size() == 32 &&
            (**receipt).Record().Bounds == expected,
        "the probe receipt and actual bounds reload without rebuilding source or geometry");
  return Report();
}

std::optional<GroundPortField> Field() {
  auto input = CanonicalGroundFixture::Load();
  auto dem = CanonicalGroundFixture::LoadTerrain();
  if (!input || !dem) { return {}; }
  auto sources = GroundPortSources::Make(input->Snapshot, input->Profiles, input->Unknown);
  auto terrain = GroundPortTerrain::Bind(dem->Input, dem->SourceDatum, dem->HeightMapping);
  if (!sources || !terrain) { return {}; }
  auto request = GroundPortFieldRequest::Prepare(*sources, input->Components[0].front());
  if (!request) { return {}; }
  auto field = GroundPortField::Make(
      std::move(*request), std::make_shared<const GroundPortTerrain>(std::move(*terrain)), .5);
  if (!field) { return {}; }
  return std::move(*field);
}

void SupportBounds(const GroundPortBodies &bodies, const Box &bounds) {
  const auto frame = TangentFrame::At(bodies.Anchor);
  for (const auto &body : bodies.Bodies) {
    for (const auto &point : body.Geometry.Points) {
      CHECK(
          Contains(bounds,
                   frame.ToEcefPosition({.EastM = point[0], .NorthM = point[1], .UpM = point[2]})),
          "actual clipped road width, crossfall and underside are all inside their spatial product "
          "bounds");
    }
    for (const auto &contact : body.TerrainContacts) {
      const double width = EarthworkApronWidthM(contact, contact.YieldM);
      double low = std::numeric_limits<double>::infinity();
      double high = -low;
      for (const double east : {contact.LowE, contact.HighE}) {
        for (const double north : {contact.LowN, contact.HighN}) {
          const double height = contact.WantsAt({.EastM = east, .NorthM = north});
          low = std::min(low, height - contact.YieldM);
          high = std::max(high, height + contact.YieldM);
        }
      }
      for (const double east : {contact.LowE - width, contact.HighE + width}) {
        for (const double north : {contact.LowN - width, contact.HighN + width}) {
          for (const double up : {low, high}) {
            CHECK(
                Contains(bounds, frame.ToEcefPosition({.EastM = east, .NorthM = north, .UpM = up})),
                "the deformation consumer's actual soft-apron/relief support is covered in ECEF");
          }
        }
      }
    }
  }
}

Vec3 ApronPoint(const GroundPortBodies &bodies) {
  const auto &stamp = bodies.Bodies.front().TerrainContacts.front();
  const auto &ring = stamp.RingEastNorthM;
  const double east = ring[2] - ring[0];
  const double north = ring[3] - ring[1];
  const double offset = .5 * stamp.ApronM / std::hypot(east, north);
  const EastNorth sample{.EastM = std::midpoint(ring[0], ring[2]) + north * offset,
                         .NorthM = std::midpoint(ring[1], ring[3]) - east * offset};
  double low = std::numeric_limits<double>::infinity();
  double high = -low;
  for (size_t at = 0; at < ring.size(); at += 2) {
    const double height = stamp.WantsAt({.EastM = ring[at], .NorthM = ring[at + 1]});
    low = std::min(low, height);
    high = std::max(high, height);
  }
  const double target = std::clamp(stamp.WantsAt(sample), low, high);
  std::array heights{target - 1};
  const std::array samples{sample};
  const std::array stamps{stamp};
  EarthworkDeformationJob job(stamps, samples, heights, 2);
  while (!job.Advance(1)) {}
  const auto result = job.Take();
  std::println(
      "GROUND_APRON sample_e={} sample_n={} target={} actual={} moved={} held={} structures={}",
      sample.EastM,
      sample.NorthM,
      target,
      heights[0],
      result.Moved,
      result.Held,
      result.Structures);
  CHECK(result.Moved == 1 && heights[0] > target - 1 && heights[0] <= target,
        "the outside-owner probe lies in actual terrain deformation support, beyond the solid "
        "footprint");
  return TangentFrame::At(bodies.Anchor)
      .ToEcefPosition({.EastM = sample.EastM, .NorthM = sample.NorthM, .UpM = target});
}

void Discovery(const GroundPortField &field, const std::string &executable) {
  const auto core =
      CellsAround(field.Domain().Anchor.LatitudeDeg, field.Domain().Anchor.LongitudeDeg, 1, 19, 16);
  CHECK(core && !core->empty(), "the test derives an owner address from the actual source field");
  if (!core || core->empty()) { return; }
  const auto base = BuildGroundPortBodies(field, {.ThicknessM = .3, .ApronM = 1000, .ReliefM = .4});
  CHECK(base.has_value(),
        "an explicit broad apron recipe builds actual native geometry and contacts");
  if (!base) { return; }
  const auto &graph = field.Source().Source->Topology();
  auto selection = SelectGroundPortCore(field,
                                        core->front(),
                                        {.SourceNodesMost = graph.Nodes().size(),
                                         .SourceEdgesMost = graph.Edges().size(),
                                         .SegmentsMost = 4096,
                                         .ContainerCapacityMost = 1048576});
  CHECK(selection && !selection->Segments.empty(),
        "the selected owner contains actual canonical source intervals");
  if (!selection || selection->Segments.empty()) { return; }
  const auto bodies = SliceGroundPortBodies(field, *base, *selection);
  CHECK(bodies && !bodies->Bodies.empty(),
        "the owner product is constructed before the independent spatial query");
  if (!bodies || bodies->Bodies.empty()) { return; }
  const auto bounds = GroundPortBodyBoundsEcef(*bodies);
  SupportBounds(*bodies, bounds);
  const auto point = ApronPoint(*bodies);
  const auto geo = GeoFromEcef(point);
  CHECK(geo.has_value(), "actual supported ECEF point has a geographic address");
  if (!geo) { return; }
  constexpr double count = uint32_t{1} << 19U;
  const auto x = static_cast<int64_t>((geo->LongitudeDeg + 180) / 360 * count);
  const auto y = static_cast<int64_t>((geo->LatitudeDeg + 90) / 180 * count);
  CHECK(std::abs(x - selection->Core.X) > 1 || std::abs(y - selection->Core.Y) > 1,
        "the actual soft-apron query is outside both the owner and a fixed one-neighbor ring");
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-tests.ground-bounds-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = (root / "index.sqlite").string();
  auto cache = AssetCache::Open(path);
  CHECK(cache.has_value(), "the existing shared native spatial index opens for bound metadata");
  if (!cache) { return; }
  const auto &receipt = bodies->ContentBinding;
  const std::string key = Sha256Hex(receipt.data(), receipt.size());
  const AssetRecord record{.Key = key,
                           .Kind = "ground-body-bounds-probe",
                           .Bounds = bounds,
                           .Package = {},
                           .ByteCount = receipt.size(),
                           .Level = static_cast<uint32_t>(selection->Core.Level),
                           .Parent = {},
                           .RequestKey = {}};
  CHECK((*cache)->Publish(std::span(&record, 1), receipt).has_value(),
        "the actual geographic body identity and geometric bounds are published as an explicit "
        "metadata probe");
  cache->reset();
  CHECK(Spawn(executable, path, key, bounds, point),
        "a fresh offline process discovers the distant owner using persisted actual bounds");
  std::filesystem::remove_all(root);
}
}

int main(int argc, char **argv) {
  if (argc == 13 && std::string_view(argv[1]) == "--offline-bounds") { return Offline(argv); }
  const auto field = Field();
  CHECK(field.has_value(),
        "the original pinned source and DEM produce the original complete affine field");
  if (field) { Discovery(*field, std::filesystem::absolute(argv[0]).string()); }
  return Report();
}
