#include "Check.h"
#include "TextTarget.h"

#include <cstdio>
#include <string>

#include <unistd.h>

using namespace outshine;

namespace {

std::string ScratchDir() {
  const char *nest = getenv("OUTSHINE_NEST");
  if (nest && *nest) { return std::string(nest) + "/"; }
  const char *tmp = getenv("TMPDIR");
  std::string dir = tmp && *tmp ? tmp : "/tmp";
  if (dir.back() != '/') dir += '/';
  return dir + "outshine-" + std::to_string(getpid()) + "-";
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

}

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

  const std::string unreachable = ScratchDir() + "outshine-no-such-directory/x.log";
  const TextTarget refused(unreachable);
  CHECK(!refused.Refusal().empty(), "a path that will not open refuses instead of swallowing");
  CHECK(refused.File() == nullptr, "and hands out no stream, so nothing can be written into it");
  CHECK(refused.Refusal().find(unreachable) != std::string::npos,
        "the refusal names the path it refused");
  CHECK(refused.Name() == unreachable, "and the target still knows what it was asked for");

  return Test::Report();
}
