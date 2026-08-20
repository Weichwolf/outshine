#ifndef GLTF_VARIANT_H
#define GLTF_VARIANT_H

#include <optional>
#include <string>

namespace outshine::Gltf {

class Document;

class VariantSelection {
public:

  VariantSelection() = default;
  explicit VariantSelection(std::string name) : Name_(std::move(name)) {}

  [[nodiscard]] bool Against(const Document &document, int &index, std::string &why) const;

private:
  std::optional<std::string> Name_;
};

}
#endif
