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
sed -n '/^CXX=/p; /^CXXSTD=/p; /^WARN=/p' test/run.sh > "$root/check.sh"
for name in PrepareCompilerCache CompileCachedObject FileStamp BinaryStamp Fresh; do
  sed -n "/^${name}()/,/^}/p" test/run.sh >> "$root/check.sh"
done
. "$root/check.sh"
cd "$root"
PrepareCompilerCache
printf '#include "header.h"\nint value() { return VALUE; }\n' > unit.cpp
printf '#define VALUE 1\n' > header.h
CompileCachedObject unit.cpp unit.o "$CXXSTD" $WARN -I. || exit 10
cp unit.o saved.o
cp -p header.h saved.h
printf '#define VALUE 2\n' > header.h
touch -r saved.h header.h
CompileCachedObject unit.cpp unit.o "$CXXSTD" $WARN -I. || exit 11
if cmp -s unit.o saved.o; then exit 12; fi
rm header.h
if CompileCachedObject unit.cpp unit.o "$CXXSTD" $WARN -I.; then exit 13; fi
cp saved.h header.h
rm unit.cpp
if CompileCachedObject unit.cpp unit.o "$CXXSTD" $WARN -I.; then exit 14; fi
OBJECTS=
mkdir -p test/harness/shared
touch test/harness/shared/check.h
touch unit.cpp unit.bin shader.glsl input.json
printf 'unit.bin: unit.cpp shader.glsl input.json\n' > unit.bin.d
printf 'compile' > unit.bin.cmd
BinaryStamp unit.bin unit.cpp > unit.bin.stamp
Fresh unit.bin compile unit.cpp || exit 15
printf 'shader revision\n' > shader.glsl
touch -t 202001010000 shader.glsl
if Fresh unit.bin compile unit.cpp; then exit 16; fi
BinaryStamp unit.bin unit.cpp > unit.bin.stamp
Fresh unit.bin compile unit.cpp || exit 17
printf 'different JSON\n' > input.json
touch -t 202001010000 input.json
if Fresh unit.bin compile unit.cpp; then exit 18; fi
BinaryStamp unit.bin unit.cpp > unit.bin.stamp
rm shader.glsl
if Fresh unit.bin compile unit.cpp; then exit 19; fi
)SH",
                          said);
  CHECK(verdict == 0, "objects and binaries reject changed or missing prerequisites of any format");
  return Report();
}
