#pragma once
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Test::Mvt {
using Bytes = std::vector<uint8_t>;

inline void Append(Bytes &out, uint8_t tag, std::span<const uint8_t> bytes) {
  assert(bytes.size() < 128);
  out.push_back(tag);
  out.push_back(static_cast<uint8_t>(bytes.size()));
  out.insert(out.end(), bytes.begin(), bytes.end());
}
}
