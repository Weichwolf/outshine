#include "AssetCache.h"
#include "StructureArtifact.h"
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

int main(int argc, const char *const *argv) {
  using namespace outshine;
  using namespace outshine::Generators;
  if (argc != 2) { return 2; }
  const auto cache = AssetCache::Open(argv[1]);
  if (!cache) { return 3; }
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
  std::string key;
  while (std::cin >> key) {
    const auto read = (*cache)->Load(key, kStructureArtifactBytesMost);
    if (!read || !*read) {
      std::cout << key << " unavailable\n";
      continue;
    }
    const auto &asset = **read;
    const auto tile = DecodeStructureArtifact(asset.Bytes(), key, 1);
    if (!tile || tile->RequestedDetail != LevelOfDetail::Shell || !tile->RequestedCell) {
      std::cout << key << " invalid\n";
    } else if (tile->SurfaceFailure || !tile->SurfaceError) {
      std::cout << key << " unqualified\n";
    } else {
      std::cout << key << " certified " << tile->SurfaceError->UpperDistanceM() << '\n';
    }
  }
  return std::cin.eof() ? 0 : 4;
}
