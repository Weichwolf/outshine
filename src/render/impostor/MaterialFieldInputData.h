#ifndef OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDINPUTDATA_H
#define OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDINPUTDATA_H
#include "MaterialFieldInput.h"
#include "GeometryChannel.h"
#include "MaterialBinding.h"
#include "MeshInput.h"
#include "RenderGeometryStorage.h"
#include "Viewing.h"
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
struct MaterialFieldInput::Data {
  std::shared_ptr<const Geometry> Source;
  RenderGeometryStorage Storage;
  RenderGeometryView View;
  std::vector<MaterialBinding> Materials;
  std::vector<float> Positions;
  std::vector<std::vector<uint8_t>> PreparedMips;
  std::array<GeometryChannel, 6> Channels;
  DrawList Draws;
  std::vector<uint32_t> Indices;
  Extent Raster;
  std::optional<Viewpoint> Capture;

  [[nodiscard]] std::expected<void, std::string> Prepare();
  [[nodiscard]] std::expected<void, std::string> PrepareTextures();
  [[nodiscard]] std::expected<void, std::string> PrepareTexture(MaterialTexture &texture,
                                                                ImageMipKind kind);
  [[nodiscard]] MeshInput Mesh() const;
  static void Emit(const void *carrying, float *into, uint32_t floats);
};
}
#endif
