#include "Species.h"

#include <cstdio>
#include <string>

namespace outshine::Clients {

bool ReadSpecies(const char *path, Generators::TreeSpecies *out) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  std::string text;
  char buf[8192];
  for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
  fclose(f);
  return !text.empty() && out->Parse(text.c_str(), text.size());
}

} // namespace outshine::Clients
