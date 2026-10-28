#ifndef OUTSHINE_RENDER_MATERIAL_MATERIALREGISTRY_H
#define OUTSHINE_RENDER_MATERIAL_MATERIALREGISTRY_H

#include "MaterialLifetime.h"
#include "MaterialRegistration.h"
#include "ResourceHandle.h"
#include "Geometry.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Render {
class GeometryRenderer;
struct MaterialSource;

class MaterialRegistry {
public:
  MaterialRegistry() = default;
  MaterialRegistry(const MaterialRegistry &) = delete;
  MaterialRegistry &operator=(const MaterialRegistry &) = delete;
  MaterialRegistry(MaterialRegistry &&) noexcept = default;
  MaterialRegistry &operator=(MaterialRegistry &&) noexcept = default;
  [[nodiscard]] std::expected<MaterialRegistration, std::string>
  Register(GeometryRenderer &geometry, GeometryRenderer *glass, Geometry source);
  [[nodiscard]] bool
  Prepare(GeometryRenderer &geometry, GeometryRenderer *glass, std::string &error);
  [[nodiscard]] bool
  Restore(GeometryRenderer &geometry, GeometryRenderer *glass, std::string &error);
  [[nodiscard]] bool Contains(const MaterialRegistration &material) const noexcept;
  void CopySourcesFrom(const MaterialRegistry &source);

private:
  struct Row {
    std::weak_ptr<const MaterialSource> Owner;
    uint32_t NextFree = kNoResourceSlot;
  };

  struct SourceRecord {
    ResourceSlotState State{};
    std::weak_ptr<const MaterialSource> Source;
    std::vector<uint32_t> Rows;
  };

  [[nodiscard]] static std::expected<MaterialSource, std::string> Resolve(Geometry source);
  [[nodiscard]] bool Write(GeometryRenderer &geometry,
                           GeometryRenderer *glass,
                           const MaterialSource &source,
                           std::string &error);
  void Collect(GeometryRenderer &geometry, GeometryRenderer *glass);
  void Retire(uint32_t which, GeometryRenderer *geometry, GeometryRenderer *glass);
  [[nodiscard]] uint32_t TakeRow();
  void GiveRow(uint32_t which);

  std::vector<Row> Rows_;
  uint32_t FirstFreeRow_ = kNoResourceSlot;
  std::vector<SourceRecord> Sources_;
  uint32_t FirstFreeSource_ = kNoResourceSlot;
  std::shared_ptr<MaterialRetirementQueue> Retirements_ =
      std::make_shared<MaterialRetirementQueue>();
  std::vector<MaterialRetirement> Retired_;

  struct Binding {
    GeometryRenderer *Geometry = nullptr;
    GeometryRenderer *Glass = nullptr;
    uint64_t GeometryRevision = 0;
    uint64_t GlassRevision = 0;
  };

  std::optional<Binding> Binding_;
};
}
#endif
