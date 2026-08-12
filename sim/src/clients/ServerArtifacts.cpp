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

Artifacts::Delivery ServerArtifacts::Png(const std::string &name, const uint8_t *rgba, int width,
                                         int height) {
  std::vector<uint8_t> png;
  if (!EncodePng(rgba, width, height, png)) {
    Log::Error("run", "png_encode_failed", {{"name", name}});
    return Delivery::Refused;
  }
  return Bytes(name, png.data(), png.size());
}

Artifacts::Delivery ServerArtifacts::Send(const std::string &name, const void *data, size_t bytes,
                                          const char *contentType) {
  Reap();   /* the finished posts leave here, so the list holds what is actually on the wire */
  Flight_.push_back(std::make_unique<HttpPost>());
  Flight_.back()->Begin(Url(name), data, bytes, contentType);
  return Delivery::InFlight;
}

Artifacts::Delivery ServerArtifacts::Bytes(const std::string &name, const void *data, size_t bytes) {
  return Send(name, data, bytes, "application/octet-stream");
}

Artifacts::Delivery ServerArtifacts::Text(const std::string &name, const std::string &text) {
  return Send(name, text.data(), text.size(), "text/plain");
}

void ServerArtifacts::Reap() {
  size_t kept = 0;
  for (size_t i = 0; i < Flight_.size(); i++) {
    switch (Flight_[i]->Take()) {
      case HttpPost::State::InFlight:
        if (kept != i) Flight_[kept] = std::move(Flight_[i]);
        kept++;
        break;
      case HttpPost::State::Refused: Refused_ = true; break;
      case HttpPost::State::Idle:
      case HttpPost::State::Delivered: break;
    }
  }
  Flight_.resize(kept);
}

Artifacts::Delivery ServerArtifacts::Settle() {
  Reap();
  if (!Flight_.empty()) return Delivery::InFlight;
  return Refused_ ? Delivery::Refused : Delivery::Complete;
}

}  // namespace outshine::Clients
