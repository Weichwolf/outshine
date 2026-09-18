#include "Check.h"
#include "Document.h"
#include "SkeletonImport.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Gltf;
  using namespace outshine::Test;

  auto temporary = (std::filesystem::temp_directory_path() / "outshine-skeleton-XXXXXX").string();
  if (mkdtemp(temporary.data()) == nullptr) {
    Unprepared("fixture directory unavailable");
    return Report();
  }
  const std::filesystem::path root(temporary);
  const std::array<float, 16> bind = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 5, 6, 7, 1};
  {
    std::ofstream bytes(root / "bind.bin", std::ios::binary);
    bytes.write(reinterpret_cast<const char *>(bind.data()), sizeof(bind));
    CHECK(bytes.good(), "inverse bind fixture written");
  }
  {
    std::ofstream file(root / "skin.gltf");
    file << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"bind.bin","byteLength":64}],
      "bufferViews":[{"buffer":0,"byteLength":64}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"MAT4"}],
      "nodes":[{},{}],"skins":[{"joints":[1],"skeleton":0,"inverseBindMatrices":0}]})";
    CHECK(file.good(), "skeleton declaration written");
  }

  std::vector<Skeleton> skeletons;
  std::string error;
  {
    Document document;
    CHECK(document.ReadFile((root / "skin.gltf").string()), document.Error().c_str());
    CHECK(ImportSkeletons(document, skeletons, error), error.c_str());
  }
  CHECK((skeletons.size() == 1 && skeletons[0].RootNode == 0 &&
         skeletons[0].JointNodes == std::vector<uint32_t>{1}),
        "native skeleton owns its root and joint node identity");
  CHECK((skeletons[0].InverseBind.size() == 1 && skeletons[0].InverseBind[0].M[0] == 2 &&
         skeletons[0].InverseBind[0].M[5] == 3 && skeletons[0].InverseBind[0].M[10] == 4 &&
         skeletons[0].InverseBind[0].M.Translation() == Vec3{{5, 6, 7}}),
        "native skeleton owns its inverse bind after the import document is gone");

  std::error_code cleanup;
  std::filesystem::remove_all(root, cleanup);
  CHECK(!cleanup, "fixture directory removed");
  return Report();
}
