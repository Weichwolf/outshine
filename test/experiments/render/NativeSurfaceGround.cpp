#include "ByteArchive.h"
#include "GroundRegionAsset.h"
#include "ReadTextFile.h"
#include "SurfaceTile.h"
#include "TangentFrame.h"
#include "TerrainClassificationMap.h"
#include "Units.h"
#include "Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>

namespace {
using namespace outshine;

constexpr size_t kRegionBytesMost = size_t{512} * 1024 * 1024;

bool Save(const std::filesystem::path &path, std::span<const uint8_t> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(output);
}

EastNorthUp AsEnu(const Vec3f &eus) {
  return {.EastM = eus[0], .NorthM = -eus[2], .UpM = eus[1]};
}

bool Sample(const SurfaceTile &tile,
            const TerrainClassificationMap &ground,
            const std::filesystem::path &output) {
  const size_t side = tile.GridSide;
  if (side == 0) { return false; }
  const auto frame = TangentFrame::At(tile.Origin);
  const auto &reference = ground.Frame();
  ByteWriter samples(side * side * (2 * sizeof(uint32_t) + 2 * sizeof(float)));
  for (const auto &vertex : std::span(tile.Vertices).first(side * side)) {
    const auto at = reference.ToLocalPosition(frame.ToEcefPosition(AsEnu(vertex.PositionM)));
    const auto material = ground.MaterialAt(at.EastM, at.NorthM);
    const auto normal = frame.ToEcefDirection(AsEnu(vertex.Normal));
    const auto incline = std::clamp(Dot(normal, reference.UpEcef()), 0.0, 1.0);
    const auto slope = static_cast<float>(std::acos(incline) / kDeg2Rad);
    const auto first = material.Class >= 0 ? material.Class : ground.UnmappedRow();
    const auto other = material.Other >= 0 ? material.Other : ground.UnmappedRow();
    if (!samples.Number(static_cast<int32_t>(first)) ||
        !samples.Number(static_cast<int32_t>(other)) ||
        !samples.Number(static_cast<float>(material.SignedEdgeM)) || !samples.Number(slope)) {
      return false;
    }
  }
  return Save(output, samples.Bytes());
}
}

int main(int argc, char *const *argv) {
  if (argc < 4) { return 2; }
  const auto bytes = ReadTextFile(argv[1], kRegionBytesMost);
  if (!bytes) { return 1; }
  const auto ground = DecodeGroundRegionAsset(
      {reinterpret_cast<const uint8_t *>(bytes->data()), bytes->size()}, kRegionBytesMost);
  if (!ground || !ground->Classes || ground->ClassPalette.empty()) { return 1; }
  const std::filesystem::path output(argv[2]);
  ByteWriter palette(ground->ClassPalette.size() * sizeof(float));
  for (const float value : ground->ClassPalette) {
    if (!palette.Number(value)) { return 1; }
  }
  if (!Save(output / "palette.f32", palette.Bytes())) { return 1; }
  for (int arg = 3; arg < argc; ++arg) {
    const std::filesystem::path source(argv[arg]);
    const auto tile = ReadSurfaceTile(argv[arg]);
    if (!tile ||
        !Sample(*tile, *ground->Classes, output / (source.filename().string() + ".ground"))) {
      std::cerr << "could not sample ground material for " << source << '\n';
      return 1;
    }
  }
  return 0;
}
