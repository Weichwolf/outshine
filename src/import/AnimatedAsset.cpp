#include "AnimatedAsset.h"

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "import/GltfImporter.h"

namespace outshine {

struct AnimatedAsset::Held {
  GltfImporter Importer;
};

AnimatedAsset::AnimatedAsset() : Held_(std::make_unique<Held>()) {}

AnimatedAsset::~AnimatedAsset() = default;
AnimatedAsset::AnimatedAsset(AnimatedAsset &&) noexcept = default;
AnimatedAsset &AnimatedAsset::operator=(AnimatedAsset &&) noexcept = default;

std::expected<void, std::string> AnimatedAsset::load(std::string_view path) {
  return Held_->Importer.load(path);
}

std::expected<void, std::string> AnimatedAsset::selectMaterialVariant(std::string_view variant) {
  return Held_->Importer.selectMaterialVariant(variant);
}

std::expected<void, std::string> AnimatedAsset::selectAnimations(std::span<const int> animations) {
  return Held_->Importer.selectAnimations(animations);
}

std::expected<void, std::string> AnimatedAsset::sampleAnimation(double seconds) {
  return Held_->Importer.sampleAnimation(seconds);
}

const Geometry &AnimatedAsset::geometry() const {
  return Held_->Importer.geometry();
}

int AnimatedAsset::animationCount() const {
  return Held_->Importer.animationCount();
}

double AnimatedAsset::durationS() const {
  return Held_->Importer.durationS();
}

int AnimatedAsset::cameraCount() const {
  return Held_->Importer.cameraCount();
}

std::expected<Camera, std::string> AnimatedAsset::camera(int index) const {
  return Held_->Importer.camera(index);
}

}
