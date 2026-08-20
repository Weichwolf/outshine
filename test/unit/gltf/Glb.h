#ifndef TEST_GLB_H
#define TEST_GLB_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace outshine::Test {

template <class Scalar> inline void Append(std::vector<uint8_t> &out, Scalar value) {
  const auto *at = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), at, at + sizeof value);
}

inline void PadTo4(std::vector<uint8_t> &out, uint8_t filler) {
  while ((out.size() % 4) != 0) { out.push_back(filler); }
}

inline std::vector<uint8_t> Glb(const std::string &json, const std::vector<uint8_t> &binary) {
  std::vector<uint8_t> jsonChunk(json.begin(), json.end());
  while ((jsonChunk.size() % 4) != 0) { jsonChunk.push_back(' '); }
  std::vector<uint8_t> binaryChunk = binary;
  while ((binaryChunk.size() % 4) != 0) { binaryChunk.push_back(0); }

  std::vector<uint8_t> out;
  const uint32_t total = 12 + 8 + static_cast<uint32_t>(jsonChunk.size()) +
                         (binaryChunk.empty() ? 0 : 8 + static_cast<uint32_t>(binaryChunk.size()));
  Append(out, uint32_t{0x46546C67});
  Append(out, uint32_t{2});
  Append(out, total);
  Append(out, static_cast<uint32_t>(jsonChunk.size()));
  Append(out, uint32_t{0x4E4F534A});
  out.insert(out.end(), jsonChunk.begin(), jsonChunk.end());
  if (!binaryChunk.empty()) {
    Append(out, static_cast<uint32_t>(binaryChunk.size()));
    Append(out, uint32_t{0x004E4942});
    out.insert(out.end(), binaryChunk.begin(), binaryChunk.end());
  }
  return out;
}

}
#endif
