#include "HostProcessor.h"
#include <cstddef>
#include <fstream>
#include <string>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace outshine {

std::string HostProcessorName() {
#ifdef __APPLE__
  size_t bytes = 0;
  if (sysctlbyname("machdep.cpu.brand_string", nullptr, &bytes, nullptr, 0) == 0 && bytes > 1) {
    std::string name(bytes, '\0');
    if (sysctlbyname("machdep.cpu.brand_string", name.data(), &bytes, nullptr, 0) == 0) {
      name.resize(bytes - 1);
      return name;
    }
  }
#else
  std::ifstream file("/proc/cpuinfo");
  std::string line;
  while (std::getline(file, line)) {
    if (line.starts_with("model name")) {
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        const auto first = line.find_first_not_of(" \t", colon + 1);
        if (first != std::string::npos) { return line.substr(first); }
      }
    }
  }
#endif
  return "unknown";
}

}
