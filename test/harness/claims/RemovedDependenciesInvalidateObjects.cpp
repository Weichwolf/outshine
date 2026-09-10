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
)SH",
                          said);
  CHECK(verdict == 0, "object freshness rejects newer or missing compiler prerequisites");
  return Report();
}
