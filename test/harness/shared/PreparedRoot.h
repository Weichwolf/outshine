#ifndef TEST_PREPAREDROOT_H
#define TEST_PREPAREDROOT_H

#include <cstdlib>
#include <string>

namespace outshine::Test {

inline constexpr const char *kPreparedKhronosPrefix = "test-render-khronos-glTF-";
inline constexpr const char *kPreparedGrownPrefix = "test-render-outshine-grown-";

inline std::string PreparedRoot() {
  const char *declared = std::getenv("TMPDIR");
  std::string root = (declared != nullptr && declared[0] != '\0') ? declared : "/tmp";
  while (root.size() > 1 && root.back() == '/') { root.pop_back(); }
  return root + "/outshine-prepared";
}

}
#endif
