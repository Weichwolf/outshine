#include "ServerArtifacts.h"

#include <vector>

#include "HttpPost.h"
#include "Log.h"
#include "Png.h"

namespace outshine::Clients {

std::string ServerArtifacts::Url(const std::string &name) const {
  std::string safe;
  for (char c : name) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    c == '-' || c == '_' || c == '.';
    safe += ok ? c : '-';
  }
  return Base_ + "/artifact/" + RunId_ + "-" + safe;
}

bool ServerArtifacts::Png(const std::string &name, const uint8_t *rgba, int width, int height) {
  std::vector<uint8_t> png;
  if (!EncodePng(rgba, width, height, png)) {
    Log::Error("run", "png_encode_failed", {{"name", name}});
    return false;
  }
  return Bytes(name, png.data(), png.size());
}

bool ServerArtifacts::Bytes(const std::string &name, const void *data, size_t bytes) {
  return HttpPost(Url(name), data, bytes, "application/octet-stream");
}

bool ServerArtifacts::Text(const std::string &name, const std::string &text) {
  return HttpPost(Url(name), text.data(), text.size(), "text/plain");
}

}  // namespace outshine::Clients
