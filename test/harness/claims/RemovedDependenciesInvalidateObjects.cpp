#include <string>

#include "Check.h"
#include "Shell.h"

int main() {
  using namespace outshine::Test;
  std::string said;
  const int verdict = Run(R"SH(
set -eu
root=$(mktemp -d "${TMPDIR:-/tmp}/outshine-dependencies.XXXXXX")
trap 'rm -rf "$root"' EXIT
sed -n '/^UpToDate()/,/^}/p' test/run.sh > "$root/check.sh"
sed -n '/^BinaryStamp()/,/^}/p' test/run.sh >> "$root/check.sh"
sed -n '/^Fresh()/,/^}/p' test/run.sh >> "$root/check.sh"
. "$root/check.sh"
cd "$root"
printf 'unit.o: unit.cpp header.h\n' > unit.d
touch -t 202001010000 unit.cpp header.h
touch -t 202001020000 unit.o
UpToDate unit.o unit.cpp || exit 10
touch -t 202001030000 header.h
if UpToDate unit.o unit.cpp; then exit 11; fi
rm header.h
if UpToDate unit.o unit.cpp; then exit 12; fi
touch -t 202001010000 header.h
rm unit.cpp
if UpToDate unit.o unit.cpp; then exit 13; fi
OBJECTS=
mkdir -p test/harness/shared
touch test/harness/shared/check.h
touch unit.cpp unit.bin shader.glsl input.json
printf 'unit.bin: unit.cpp shader.glsl input.json\n' > unit.bin.d
printf 'compile' > unit.bin.cmd
BinaryStamp unit.bin unit.cpp > unit.bin.stamp
Fresh unit.bin compile unit.cpp || exit 14
touch -t 202001010000 shader.glsl
if Fresh unit.bin compile unit.cpp; then exit 15; fi
BinaryStamp unit.bin unit.cpp > unit.bin.stamp
Fresh unit.bin compile unit.cpp || exit 16
touch -t 202001010000 input.json
if Fresh unit.bin compile unit.cpp; then exit 17; fi
BinaryStamp unit.bin unit.cpp > unit.bin.stamp
rm shader.glsl
if Fresh unit.bin compile unit.cpp; then exit 18; fi
)SH",
                          said);
  CHECK(verdict == 0, "objects and binaries reject changed or missing prerequisites of any format");
  return Report();
}
