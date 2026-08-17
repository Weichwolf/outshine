/* WHERE A CASE'S PREPARED FILES ARE, AND IT IS NOT THE TREE.
 *
 * `CLAUDE.md`: every artefact goes to the system temp directory, never into the tree -- a repository is
 * what is declared and what is built from it. The preparer writes each case's fetched buffers, images,
 * converted `.blend` and every oracle product under this root, and `test/run.sh` hands the runner the
 * same directory. This header is the third and last consumer, so the three agree by construction
 * rather than by three copies of a path.
 *
 * THE LEAF IS THE CASE'S OWN PATH WITH ITS SEPARATORS FLATTENED -- `test/khronos/glTF/Box` becomes
 * `test-khronos-glTF-Box`. It is derivable from either end without a table, which is what lets a test
 * ask *which corpus is this case in* by looking at the prefix rather than by carrying a list. */
#ifndef TEST_PREPAREDROOT_H
#define TEST_PREPAREDROOT_H

#include <cstdlib>
#include <string>

namespace outshine::Test {

inline constexpr const char *kPreparedKhronosPrefix = "test-khronos-glTF-";
inline constexpr const char *kPreparedGrownPrefix = "test-outshine-render-";

/* `TMPDIR` WITH `/tmp` BEHIND IT, which is what `test/run.sh` resolves and what Python's
 * `tempfile.gettempdir()` returns first. A trailing separator is stripped so the join below produces
 * one path rather than two spellings of it. */
inline std::string PreparedRoot() {
  const char *declared = std::getenv("TMPDIR");
  std::string root = (declared != nullptr && declared[0] != '\0') ? declared : "/tmp";
  while (root.size() > 1 && root.back() == '/') { root.pop_back(); }
  return root + "/outshine-prepared";
}

} // namespace outshine::Test
#endif
