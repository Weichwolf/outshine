#ifndef OUTSHINE_RENDER_STAGES_VERTEXUPLOAD_H
#define OUTSHINE_RENDER_STAGES_VERTEXUPLOAD_H
#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>
#include "SubjectTypes.h"
#include "SubjectResidency.h"

namespace outshine::Render {
namespace Says {
inline constexpr std::string_view kVertexUploadTooLarge =
    "vertex upload range exceeds GPU byte addressing";
}

struct VertexStreamUpload {
  SubjectResidency::Stream Which;
  SubjectStream Source;
  bool Carried;
  uint32_t Components;
};

[[nodiscard]] inline std::expected<SubjectResidency::Crossing, std::string_view>
VertexCrossing(VertexStreamUpload upload, SubjectResidency::Range vertices) {
  SubjectResidency::Crossing crossing{.Which = upload.Which, .Usage = SDL_GPU_BUFFERUSAGE_VERTEX};
  if (!upload.Carried) { return crossing; }
  const uint64_t stride = uint64_t{upload.Components} * sizeof(float);
  const uint64_t end = uint64_t{vertices.First} + vertices.Count;
  if (stride == 0 || end > std::numeric_limits<uint32_t>::max() / stride) {
    return std::unexpected(Says::kVertexUploadTooLarge);
  }
  crossing.From = upload.Source.From;
  crossing.Writes = upload.Source.Writes;
  crossing.Carrying = upload.Source.Carrying;
  crossing.Bytes = static_cast<uint32_t>(vertices.Count * stride);
  crossing.Offset = static_cast<uint32_t>(vertices.First * stride);
  return crossing;
}
}
#endif
