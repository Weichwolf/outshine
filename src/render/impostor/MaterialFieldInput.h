#ifndef OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDINPUT_H
#define OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDINPUT_H
#include "Extent.h"
#include "Geometry.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <string>

namespace outshine {
class ImpostorAtlas;
}

namespace outshine::Render {
class MaterialFieldPass;

class MaterialFieldInput {
public:
  // CPU preparation may run on a content worker; all geometry and lights use the field frame.
  [[nodiscard]] static std::expected<std::shared_ptr<const MaterialFieldInput>, std::string>
  Prepare(Geometry source, Extent raster);
  // Stored samples require the same orthographic rays and clipping volume.
  [[nodiscard]] static std::expected<std::shared_ptr<const MaterialFieldInput>, std::string>
  Prepare(const ImpostorAtlas &source, size_t view, size_t sourceBytesMost);
  ~MaterialFieldInput();
  MaterialFieldInput(const MaterialFieldInput &) = delete;
  MaterialFieldInput &operator=(const MaterialFieldInput &) = delete;

private:
  friend class MaterialFieldPass;
  struct Data;
  explicit MaterialFieldInput(std::unique_ptr<Data> data) noexcept;
  std::unique_ptr<Data> Data_;
};
}
#endif
