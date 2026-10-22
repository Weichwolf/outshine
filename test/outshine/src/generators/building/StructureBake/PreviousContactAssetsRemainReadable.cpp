#include "Check.h"
#include "PreparedStructureCodec.h"
#include "Sha256.h"
#include "StructureArtifact.h"
#include <cstdint>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

namespace {
std::vector<uint8_t> Read(const std::string &name) {
  std::ifstream file("test/outshine/fixtures/building-contacts/" + name,
                     std::ios::binary | std::ios::ate);
  if (!file || file.tellg() <= 0) { return {}; }
  std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg()));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return file ? bytes : std::vector<uint8_t>{};
}

bool PreviousContacts(const std::optional<PreparedStructureTile> &tile) {
  return tile && tile->Structures.size() == 3 && tile->Structures.front().Standing.TerrainCeiling &&
         tile->Structures.front().Standing.TerrainContacts.PartsView().empty() &&
         !tile->Structures[1].Standing.TerrainCeiling &&
         !tile->Structures[2].Standing.TerrainCeiling;
}
}

int main() {
  const auto prepared = DecodePreparedStructureTile(Read("prepared-v10.bin"), 8u << 20u);
  CHECK(PreviousContacts(prepared), "the unchanged original full model format remains readable");
  const auto index = DecodePreparedStructureIndex(Read("index-v9.bin"), 8u << 20u);
  CHECK(PreviousContacts(index) && index->Surfaces.front().PreparedSelection(),
        "the unchanged original metadata-only format retains its legacy physical plane");
  const auto lod =
      DecodeStructureArtifact(Read("lod-v11.bin"), Sha256Hex("ground-contact-asset"), 1);
  CHECK(lod && lod->Prints.size() == 3 && lod->Prints.front().TerrainCeiling &&
            lod->Prints.front().TerrainContacts.PartsView().empty(),
        "the unchanged original LOD product retains a usable legacy contact");
  return Report();
}
