#include "Variant.h"

#include "Document.h"
#include <string>
#include <vector>
#include <cstddef>

namespace outshine::Gltf {

bool VariantSelection::Against(const Document &document, int &index, std::string &why) const {
  if (!Name_.has_value()) {
    index = -1;
    return true;
  }
  const std::vector<std::string> &declared = document.Variants();
  for (size_t at = 0; at < declared.size(); ++at) {
    if (declared[at] == *Name_) {
      index = static_cast<int>(at);
      return true;
    }
  }
  why = "declares the material variant '" + *Name_ + "' and the file carries ";
  if (declared.empty()) {
    why += "no KHR_materials_variants at all";
    return false;
  }
  for (size_t at = 0; at < declared.size(); ++at) {
    why += ((at != 0u) ? ", '" : "'") + declared[at] + "'";
  }
  return false;
}

} // namespace outshine::Gltf
