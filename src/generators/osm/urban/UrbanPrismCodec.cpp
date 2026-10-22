#include "UrbanPrismCodec.h"
#include "BinaryValueArchive.h"
#include "math/Mat4.h"
#include "math/Vec3.h"
#include <scene/PrismGrid.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr uint64_t kFormat = 0x31534d535250534fULL;
constexpr size_t kHeaderBytes = 16;
constexpr size_t kPatchBytes = 164;

bool ValidPlacement(const UrbanPrismPlacement &placement) {
  const Mat4 &m = placement.EcefFromLocal;
  if (!std::ranges::all_of(m, [](double x) { return std::isfinite(x); }) || m.At(3, 0) != 0 ||
      m.At(3, 1) != 0 || m.At(3, 2) != 0 || m.At(3, 3) != 1 ||
      !std::isfinite(placement.MaxCornerErrorM) || placement.MaxCornerErrorM < 0) {
    return false;
  }
  const Vec3 x{{m.At(0, 0), m.At(1, 0), m.At(2, 0)}};
  const Vec3 y{{m.At(0, 1), m.At(1, 1), m.At(2, 1)}};
  const Vec3 z{{m.At(0, 2), m.At(1, 2), m.At(2, 2)}};
  const double determinant = Dot(Cross(x, y), z);
  return std::isfinite(determinant) && determinant != 0;
}

struct EncodedShape {
  size_t Samples = 0;
  size_t Patches = 0;
};

bool Fits(EncodedShape shape, size_t budget) {
  return budget >= kHeaderBytes && shape.Samples <= (budget - kHeaderBytes) / sizeof(PrismSpan) &&
         shape.Patches <= (budget - kHeaderBytes - shape.Samples * sizeof(PrismSpan)) / kPatchBytes;
}

bool Valid(const UrbanPrisms &product) {
  return product.Geometry.Patches.size() == product.Placements.size() &&
         std::ranges::all_of(product.Placements, ValidPlacement) &&
         InspectPrismGrid(product.Geometry).has_value();
}

bool WritePatch(BinaryValueWriter &writer,
                const PrismPatch &patch,
                const UrbanPrismPlacement &placement) {
  return writer(patch.FirstSample,
                patch.Columns,
                patch.Rows,
                patch.PitchX,
                patch.PitchZ,
                patch.FillFraction,
                placement.EcefFromLocal.Column,
                placement.MaxCornerErrorM);
}

bool ReadPatch(BinaryValueReader &reader, PrismPatch &patch, UrbanPrismPlacement &placement) {
  return reader(patch.FirstSample,
                patch.Columns,
                patch.Rows,
                patch.PitchX,
                patch.PitchZ,
                patch.FillFraction,
                placement.EcefFromLocal.Column,
                placement.MaxCornerErrorM);
}
}

std::expected<std::vector<uint8_t>, std::string> EncodeUrbanPrisms(const UrbanPrisms &product,
                                                                   size_t bytesMost) {
  const size_t count = product.Geometry.Samples ? product.Geometry.Samples->size() : 0;
  const size_t patches = product.Geometry.Patches.size();
  const size_t budget = std::min(bytesMost, kUrbanPrismBytesMost);
  if (!Fits({.Samples = count, .Patches = patches}, budget)) {
    return std::unexpected("native urban-prism encoding exceeds byte budget");
  }
  if (!Valid(product)) { return std::unexpected("invalid native urban-prism product"); }
  BinaryValueWriter writer(budget);
  writer.Out.Reserve(kHeaderBytes + count * sizeof(PrismSpan) + patches * kPatchBytes);
  if (!writer(kFormat, static_cast<uint32_t>(count), static_cast<uint32_t>(patches))) {
    return std::unexpected("could not encode native urban-prism header");
  }
  if (product.Geometry.Samples) {
    for (const auto sample : *product.Geometry.Samples) {
      if (!writer(sample.Lower, sample.Upper)) {
        return std::unexpected("could not encode native urban-prism samples");
      }
    }
  }
  for (size_t index = 0; index < patches; ++index) {
    if (!WritePatch(writer, product.Geometry.Patches[index], product.Placements[index])) {
      return std::unexpected("could not encode native urban-prism patch");
    }
  }
  return std::move(writer.Out).TakeBytes();
}

std::expected<UrbanPrisms, std::string> DecodeUrbanPrisms(std::span<const uint8_t> bytes,
                                                          size_t bytesMost) {
  if (bytes.size() > std::min(bytesMost, kUrbanPrismBytesMost)) {
    return std::unexpected("native urban-prism input exceeds byte budget");
  }
  BinaryValueReader reader(bytes);
  uint64_t format = 0;
  uint32_t count = 0;
  uint32_t patches = 0;
  if (!reader(format, count, patches) || format != kFormat ||
      !Fits({.Samples = count, .Patches = patches}, bytes.size()) ||
      kHeaderBytes + uint64_t{count} * sizeof(PrismSpan) + uint64_t{patches} * kPatchBytes !=
          bytes.size()) {
    return std::unexpected("invalid native urban-prism header or extent");
  }
  auto samples = std::make_shared<std::vector<PrismSpan>>(count);
  for (auto &sample : *samples) {
    if (!reader(sample.Lower, sample.Upper)) {
      return std::unexpected("invalid native urban-prism samples");
    }
  }
  UrbanPrisms product;
  product.Geometry.Samples = std::move(samples);
  product.Geometry.Patches.resize(patches);
  product.Placements.resize(patches);
  for (size_t index = 0; index < patches; ++index) {
    if (!ReadPatch(reader, product.Geometry.Patches[index], product.Placements[index])) {
      return std::unexpected("invalid native urban-prism patch");
    }
  }
  if (!Valid(product)) { return std::unexpected("invalid decoded native urban-prism product"); }
  return product;
}
}
