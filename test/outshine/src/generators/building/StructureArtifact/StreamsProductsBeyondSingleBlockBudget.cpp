#include "StructureArtifact.h"
#include "Check.h"
#include <algorithm>
#include <cstdio>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::tmpfile(), &std::fclose);
  CHECK(file != nullptr, "temporary artifact stream opens");
  if (!file) { return Report(); }
  BakedTile product;
  product.RequestedDetail = LevelOfDetail::Fine;
  product.Built.WallCorners.resize(kStructureArtifactBytesMost / sizeof(StoredVertex) + 1);
  product.Built.WallCorners.front().pos[0] = 17.0f;
  product.Built.WallCorners.back().pos[2] = 29.0f;
  constexpr size_t blockBytes = 4096;
  size_t written = 0;
  size_t largest = 0;
  const auto encoded = WriteStructureProduct(
      product,
      [&](std::span<const uint8_t> bytes) {
        written += bytes.size();
        largest = std::max(largest, bytes.size());
        return std::fwrite(bytes.data(), 1, bytes.size(), file.get()) == bytes.size();
      },
      blockBytes);
  CHECK(encoded.has_value() && written > kStructureArtifactBytesMost && largest <= blockBytes,
        "large product streams without enlarging its transient block budget");
  std::rewind(file.get());
  const auto source = [&](std::span<uint8_t> bytes) {
    return std::fread(bytes.data(), 1, bytes.size(), file.get()) == bytes.size();
  };
  const size_t resident = product.Built.WallCorners.size() * sizeof(StoredVertex);
  CHECK(!ReadStructureProduct(
            source, {.EncodedBytes = written, .ResidentBytesMost = resident - 1}, 0),
        "native residency budget is enforced independently of encoded block size");
  std::rewind(file.get());
  const auto decoded =
      ReadStructureProduct(source, {.EncodedBytes = written, .ResidentBytesMost = resident}, 0);
  CHECK(decoded && decoded->Built.WallCorners.size() == product.Built.WallCorners.size() &&
            decoded->Built.WallCorners.front().pos[0] == 17.0f &&
            decoded->Built.WallCorners.back().pos[2] == 29.0f &&
            decoded->RequestedDetail == product.RequestedDetail,
        "all blocks reconstruct the complete product under its native byte budget");
  size_t accepted = 0;
  const auto interrupted = WriteStructureProduct(
      product, [&](std::span<const uint8_t>) { return ++accepted < 3; }, blockBytes);
  CHECK(!interrupted && interrupted.error() == StructureArtifactError::WriteFailed && accepted == 3,
        "failed block publication stops encoding immediately");
  return Report();
}
