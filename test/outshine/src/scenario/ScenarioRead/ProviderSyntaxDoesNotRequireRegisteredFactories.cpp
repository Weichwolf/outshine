#include "Check.h"
#include "ScenarioRead.h"
#include <array>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document document;
  std::string error;
  const std::string custom = "<scenario name='custom'><providers><provider kind='private-domain' "
                             "dataset='arbitrary' schema='private' sha256='private-capture'/>"
                             "</providers></scenario>";
  CHECK(ReadScenario(custom.data(), custom.size(), document, error) &&
            document.Providers.size() == 1 && document.Providers.front().Kind == "private-domain" &&
            document.Providers.front().Schema == "private" &&
            document.Providers.front().PayloadSha256 == "private-capture",
        "the syntax parser retains provider metadata without loading domain factories");
  const std::array invalid{
      "<scenario><providers><provider kind='osm' westDeg='8'/></providers></scenario>",
      "<scenario><providers><provider kind='osm' westDeg='nan' southDeg='1' "
      "eastDeg='9' northDeg='2'/></providers></scenario>",
      "<scenario><providers><provider kind='same'/><provider kind='same'/>"
      "</providers></scenario>"};
  for (const std::string_view source : invalid) {
    CHECK(!ReadScenario(source.data(), source.size(), document, error) && !error.empty() &&
              document.Named.Name == "custom" && document.Providers.size() == 1 &&
              document.Providers.front().Kind == "private-domain",
          "malformed declarations still fail atomically before domain setup");
  }
  return Report();
}
