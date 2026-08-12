/* A RUN WHOSE LOG CANNOT BE WRITTEN HAS TO SAY SO. What this replaces was a collector on the far end
 * of a POST: with nothing listening, `demo/frame` wrote 674 lines, delivered none and exited 0. The
 * destination is now the consumer's to name, so the failure it can name wrongly is a path — and the
 * claim below is that a path which will not open yields no stream and a reason that names it, never
 * a stream that swallows.
 *
 * DECIDABLE, not agreement: the three cases are a stream the process always has, a path this test
 * writes and reads back, and a path under a directory that does not exist. */
#include "Check.h"
#include "TextTarget.h"

#include <cstdio>
#include <string>

using namespace outshine;

namespace {

std::string ScratchDir() {
  const char *tmp = getenv("TMPDIR");
  std::string dir = tmp && *tmp ? tmp : "/tmp";
  if (dir.back() != '/') dir += '/';
  return dir;
}

std::string ReadBack(const std::string &path) {
  std::string out;
  std::FILE *f = std::fopen(path.c_str(), "r");
  if (!f) return out;
  char buf[256];
  while (std::fgets(buf, sizeof buf, f)) out += buf;
  std::fclose(f);
  return out;
}

}  // namespace

int main() {
  Test::Covers("the library owns its log: a consumer names a path, stdout or stderr, and a "
               "destination that will not open says so");

  const TextTarget console(TextStream::Stdout);
  CHECK(console.Refusal().empty(), "a process stream always opens");
  CHECK(console.File() == stdout, "TextStream::Stdout is the process's own stdout");
  CHECK(console.Name() == "stdout", "a stream names itself the way a consumer spells it");

  const TextTarget errors(TextStream::Stderr);
  CHECK(errors.File() == stderr, "TextStream::Stderr is the process's own stderr");
  CHECK(errors.Name() == "stderr", "and it names itself too");

  const std::string path = ScratchDir() + "outshine-texttarget-test.log";
  std::remove(path.c_str());
  {
    const TextTarget file(path);
    CHECK(file.Refusal().empty(), "a writable path opens");
    CHECK(file.File() != nullptr, "and hands out a stream");
    CHECK(file.Name() == path, "a path names itself by that path");
    if (file.File()) std::fputs("one line\n", file.File());
  }
  CHECK(ReadBack(path) == "one line\n",
        "the handle closes with the object, so the bytes are on disc without anyone flushing");
  std::remove(path.c_str());

  /* The one case the deleted collector could not report about itself. */
  const std::string unreachable = ScratchDir() + "outshine-no-such-directory/x.log";
  const TextTarget refused(unreachable);
  CHECK(!refused.Refusal().empty(), "a path that will not open refuses instead of swallowing");
  CHECK(refused.File() == nullptr, "and hands out no stream, so nothing can be written into it");
  CHECK(refused.Refusal().find(unreachable) != std::string::npos,
        "the refusal names the path it refused");
  CHECK(refused.Name() == unreachable, "and the target still knows what it was asked for");

  return Test::Report();
}
