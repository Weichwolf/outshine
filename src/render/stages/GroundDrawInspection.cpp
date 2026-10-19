#include "TerrainRenderer.h"
#include "ReadbackTransfer.h"
#include <chrono>
#include <cstring>
#include <expected>

namespace outshine::Render {
Native::Result<TerrainRenderer::DrawArguments> TerrainRenderer::ReadDrawArguments() const {
  DrawArguments result;
  result.SkirtInstances = Drawn();
  if (result.SkirtInstances == 0) { return result; }
  if (!Selection_.Ready()) {
    result.Patches[0] = {.IndexCount = kPatchIndices,
                         .InstanceCount = result.SkirtInstances * kPatchCount};
    return result;
  }
  if (Device_ == nullptr || Selection_.Arguments() == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground draw arguments are unavailable"});
  }
  auto read = ReadbackTransfer::Start(
      *Device_,
      *Selection_.Arguments(),
      {.Offset = Selection_.ArgumentOffset(), .Bytes = sizeof(result.Patches)});
  if (!read) { return std::unexpected(read.error()); }
  const auto mapped = read->Wait(*Device_, std::chrono::seconds(5));
  if (!mapped) { return std::unexpected(mapped.error()); }
  if (!mapped->has_value()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                         .Message = "ground draw argument readback timed out"});
  }
  std::memcpy(result.Patches.data(), mapped->value().data(), sizeof(result.Patches));
  return result;
}
}
