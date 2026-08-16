/* WHICH CODE A MEASUREMENT BELONGS TO, answered from the sources and not from the toolchain
 * (board:1157, board:1187).
 *
 * A BINARY'S HASH IS A BUILD IDENTITY AND NOT A SOURCE ONE. Two builds of one unchanged tree differ
 * -- `ar` embeds mtimes and the linker stamps a fresh `LC_UUID` -- so a frame number pinned to a
 * binary hash says *this run happened* and not *this code produced it*, which is the only question
 * the field was ever quoted for. Two runs of one source get two hashes, and a reader comparing them
 * cannot tell an unchanged tree from a changed one.
 *
 * THE POPULATION IS PART OF THE CLAIM AND IS STATED WITH THE NUMBER: every regular file under `src/`
 * and every regular file under `test/frame/`. `src/` is the library entire (CLAUDE.md) and carries no
 * derived artefact -- unlike `test/`, whose corpus and whose render cases are fetched and built and
 * would make a walk of it a measurement of what happened to be on disk. `test/frame/` is the
 * instrument itself, which decides a duration as surely as the library does.
 *
 * WHAT IT DOES NOT COVER, said rather than implied: the compiler, its flags, and the harness that
 * chose them. A digest cannot reach those, so a comparison across two builds is a comparison of
 * sources ONLY WHERE the toolchain is held fixed, and that is the reader's to hold. A file dropped
 * under `src/` that nothing compiles also moves the digest -- the safe direction, because it reads as
 * *the code changed* rather than as *the code is the same*. */
#ifndef SOURCEDIGEST_H
#define SOURCEDIGEST_H

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Sha256.h"

namespace outshine::Test {

struct SourceIdentity {
  std::string Digest;
  long Files = 0;
  long Bytes = 0;
  /* WHEN THE NEWEST FILE OF THE POPULATION WAS LAST WRITTEN, in seconds since the epoch. THE DIGEST
   * IS READ AT RUN TIME FROM THE WORKING TREE AND NOT BAKED INTO THE BINARY, so a tree edited between
   * the compile and the run publishes a digest of code the binary does not contain -- which happened
   * here the first time this instrument was run beside an editor. Printed against the binary's own
   * timestamp it is visible; it is not a verdict, because the population is deliberately WIDER than
   * what the binary links and a source outside the link set may legitimately be newer. */
  long long NewestModified = 0;
};

/* THE DIGEST OF ONE FILE'S BYTES PAIRED WITH ITS PATH, so that moving a file changes the digest.
 * Hashing the concatenated bytes alone would not: two files whose contents were swapped hash the
 * same, and so does one file split into two. */
inline void DigestTree(const std::filesystem::path &root, const std::filesystem::path &relativeTo,
                       std::vector<std::string> &lines, long &bytes, long long &newest) {
  std::error_code failed;
  for (std::filesystem::recursive_directory_iterator at(root, failed), end; at != end;
       at.increment(failed)) {
    if (failed) { return; }
    if (!at->is_regular_file(failed) || failed) { continue; }
    const auto written = std::filesystem::last_write_time(at->path(), failed);
    if (!failed) {
      newest = std::max(newest, (long long)std::chrono::duration_cast<std::chrono::seconds>(
                                    written.time_since_epoch())
                                    .count());
    }
    std::ifstream file(at->path(), std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    bytes += (long)content.size();
    lines.push_back(std::filesystem::relative(at->path(), relativeTo).generic_string() + " " +
                    Sha256Hex(content) + "\n");
  }
}

[[nodiscard]] inline SourceIdentity SourcesUnderTest(void) {
  SourceIdentity out;
  std::vector<std::string> lines;
  DigestTree("src", ".", lines, out.Bytes, out.NewestModified);
  DigestTree("test/frame", ".", lines, out.Bytes, out.NewestModified);
  std::sort(lines.begin(), lines.end());
  std::string material;
  for (const std::string &line : lines) { material += line; }
  out.Files = (long)lines.size();
  out.Digest = Sha256Hex(material);
  return out;
}

} // namespace outshine::Test
#endif
