#include <cstdio>
#include <string>

#include "Check.h"
#include "Shell.h"

int main() {
  using namespace outshine::Test;
  std::string said;
  const int verdict = Run(R"SH(
set -eu
root=$(mktemp -d "${TMPDIR:-/tmp}/outshine-archive-members.XXXXXX")
trap 'rm -rf "$root"' EXIT
sed -n '/^ArchiveFresh()/,/^}/p' test/run.sh > "$root/check.sh"
. "$root/check.sh"
cd "$root"
mkdir objects
for object in first second third; do
  printf 'int %s;\n' "$object" > "objects/$object.cpp"
  ${CXX:-c++} -c "objects/$object.cpp" -o "objects/$object.o"
done
touch -t 202001010000 objects/*.o
if ArchiveFresh library.a objects/first.o; then exit 10; fi
ar rcs library.a objects/first.o objects/second.o
touch -t 202001020000 library.a
ArchiveFresh library.a objects/first.o objects/second.o || exit 11
if ArchiveFresh library.a objects/first.o; then exit 12; fi
if ArchiveFresh library.a objects/first.o objects/second.o objects/third.o; then exit 13; fi
mv objects/second.o objects/renamed.o
if ArchiveFresh library.a objects/first.o objects/renamed.o; then exit 14; fi
if ArchiveFresh library.a objects/first.o objects/second.o; then exit 15; fi
mv objects/renamed.o objects/second.o
touch -t 202001030000 objects/second.o
if ArchiveFresh library.a objects/first.o objects/second.o; then exit 16; fi
touch -t 202001010000 objects/second.o
ArchiveFresh library.a objects/first.o objects/second.o || exit 17
printf 'not an archive' > broken.a
if ArchiveFresh broken.a objects/first.o; then exit 18; fi
rm library.a
ar rcs library.a objects/first.o
ArchiveFresh library.a objects/first.o || exit 19
ar p library.a first.o | cmp - objects/first.o || exit 20
if ArchiveFresh library.a objects/first.o objects/first.o; then exit 21; fi
)SH",
                          said);
  if (verdict != 0) { std::fputs(said.c_str(), stdout); }
  CHECK(verdict == 0,
        "real archives follow current membership, object existence and modification time");
  return Report();
}
