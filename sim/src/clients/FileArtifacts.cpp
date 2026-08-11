#include "FileArtifacts.h"

#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
#include <vector>

#include "Log.h"
#include "Png.h"

namespace outshine::Clients {

std::string FileArtifacts::Resolve(const std::string &name) const {
  if (!name.empty() && name[0] == '/') return name;
  return Root_.empty() ? name : Root_ + "/" + name;
}

bool FileArtifacts::MakeDir(const std::string &name) {
  return mkdir(Resolve(name).c_str(), 0755) == 0 || errno == EEXIST;
}

Artifacts::Delivery FileArtifacts::Png(const std::string &name, const uint8_t *rgba, int width,
                                       int height) {
  std::vector<uint8_t> png;
  if (!EncodePng(rgba, width, height, png)) {
    Log::Error("run", "png_encode_failed", {{"name", name}});
    return Delivery::Refused;
  }
  return Bytes(name, png.data(), png.size());
}

Artifacts::Delivery FileArtifacts::Bytes(const std::string &name, const void *data, size_t bytes) {
  const std::string path = Resolve(name);
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) {
    Log::Error("run", "artifact_open_failed", {{"path", path}});
    return Delivery::Refused;
  }
  const size_t n = fwrite(data, 1, bytes, f);
  fclose(f);
  if (n != bytes) {
    Log::Error("run", "artifact_short_write", {{"path", path}, {"wrote", (double)n},
        {"bytes", (double)bytes}});
    return Delivery::Refused;
  }
  return Delivery::Complete;
}

Artifacts::Delivery FileArtifacts::Text(const std::string &name, const std::string &text) {
  return Bytes(name, text.data(), text.size());
}

}  // namespace outshine::Clients
