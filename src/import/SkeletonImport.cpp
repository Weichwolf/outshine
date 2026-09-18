#include "SkeletonImport.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Document.h"

namespace outshine::Gltf {

bool ImportSkeletons(const Document &document, std::vector<Skeleton> &out, std::string &error) {
  std::vector<Skeleton> candidate;
  candidate.reserve(document.Skins().size());
  for (const Skin &declared : document.Skins()) {
    Skeleton native;
    native.RootNode = declared.Skeleton;
    native.JointNodes.reserve(declared.Joints.size());
    native.InverseBind.reserve(declared.Joints.size());
    for (size_t joint = 0; joint < declared.Joints.size(); ++joint) {
      native.JointNodes.push_back(static_cast<uint32_t>(declared.Joints[joint]));
      if (declared.InverseBind.empty()) {
        native.InverseBind.push_back(AffineTransform::Identity());
        continue;
      }
      Mat4 bind;
      std::copy_n(
          declared.InverseBind.begin() + static_cast<ptrdiff_t>(joint * 16), 16, bind.begin());
      native.InverseBind.push_back(AffineTransform::FromColumnMajor(bind));
    }
    if (native.JointNodes.empty() || native.JointNodes.size() != native.InverseBind.size()) {
      error = document.Path() + ": a validated skin did not convert to a complete native skeleton";
      return false;
    }
    candidate.push_back(std::move(native));
  }
  out = std::move(candidate);
  error.clear();
  return true;
}

}
