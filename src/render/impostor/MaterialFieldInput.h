#ifndef OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDINPUT_H
#define OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDINPUT_H
#include "Extent.h"
#include "Geometry.h"
#include <expected>
#include <memory>
#include <string>

namespace outshine::Render {
class MaterialFieldPass;

class MaterialFieldInput {
public:
  // CPU preparation may run on a content worker; all geometry and lights use the field frame.
  [[nodiscard]] static std::expected<std::shared_ptr<const MaterialFieldInput>, std::string>
  Prepare(Geometry source, Extent raster);
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
