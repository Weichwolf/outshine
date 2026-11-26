#ifndef OUTSHINE_TEST_GROUPIMPOSTORPROBEFIELDS_H
#define OUTSHINE_TEST_GROUPIMPOSTORPROBEFIELDS_H

#include "ByteArchive.h"
#include "GroupImpostorBake.h"
#include "GroupImpostorView.h"
#include "ImpostorAtlas.h"
#include "WriteFileAtomically.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace outshine::GroupProbe {

inline bool SaveView(const std::filesystem::path &path, const ImpostorAtlas::View &view) {
  constexpr size_t kTexelBytes = sizeof(uint32_t) + 9 * sizeof(float);
  ByteWriter output(view.Texels.size() * kTexelBytes);
  for (size_t at = 0; at < view.Texels.size(); ++at) {
    const auto &texel = view.Texels[at];
    const auto &material = view.Materials[at];
    if (!output.Number(texel.Surface) || !output.Number(texel.Depth)) { return false; }
    for (const float component : texel.Normal) {
      if (!output.Number(component)) { return false; }
    }
    for (const float component : material.BaseColour) {
      if (!output.Number(component)) { return false; }
    }
    if (!output.Number(material.Roughness) || !output.Number(material.Metalness)) { return false; }
  }
  return WriteFileAtomically(path.string(), std::as_bytes(output.Bytes())).has_value();
}

inline bool SaveCard(const std::filesystem::path &path,
                     const ImpostorAtlas &atlas,
                     size_t view,
                     uint32_t side) {
  const auto card = Render::BakeGroupImpostorView(atlas, view, side);
  if (!card) { return false; }
  const auto bytes = EncodeGroupImpostorView(*card);
  if (!bytes || !DecodeGroupImpostorView(*bytes)) { return false; }
  return WriteFileAtomically(path.string(), std::as_bytes(std::span(*bytes))).has_value();
}

}
#endif
