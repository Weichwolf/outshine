#include "Logging.h"
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const LogField minimum("min", std::numeric_limits<long>::min());
  const LogField maximum("max", std::numeric_limits<long>::max());
  CHECK(minimum.Value == (sizeof(long) == 8 ? "-9223372036854775808" : "-2147483648"),
        "signed long minimum is exact decimal text without floating conversion");
  CHECK(maximum.Value == (sizeof(long) == 8 ? "9223372036854775807" : "2147483647"),
        "signed long maximum preserves every integer digit");
  CHECK(LogField("negative", -42L).Value == "-42", "signed long keeps the sign");
  CHECK(LogField("integer", -42).Value == "-42", "ordinary integers retain their overload");
  CHECK(LogField("wide", -42LL).Value == "-42", "long long retains its overload");
  CHECK(LogField("bool", true).Value == "1" && LogField("bool", false).Value == "0",
        "only actual booleans select boolean formatting");
  CHECK(LogField("text", "payload").Value == "payload", "text does not select an integer");
  return Report();
}
