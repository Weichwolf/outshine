#include "Types.h"

#include <cstring>

namespace outshine::Gltf {

size_t ComponentBytes(ComponentType component) {
  switch (component) {
  case ComponentType::Int8:
  case ComponentType::UInt8: return 1;
  case ComponentType::Int16:
  case ComponentType::UInt16: return 2;
  case ComponentType::UInt32:
  case ComponentType::Float32: return 4;
  }
  return 0;
}

size_t ElementRows(ElementType element) {
  switch (element) {
  case ElementType::Scalar: return 1;
  case ElementType::Vec2: return 2;
  case ElementType::Vec3: return 3;
  case ElementType::Vec4: return 4;
  case ElementType::Mat2: return 2;
  case ElementType::Mat3: return 3;
  case ElementType::Mat4: return 4;
  }
  return 0;
}

size_t ElementColumns(ElementType element) {
  switch (element) {
  case ElementType::Mat2: return 2;
  case ElementType::Mat3: return 3;
  case ElementType::Mat4: return 4;
  case ElementType::Scalar:
  case ElementType::Vec2:
  case ElementType::Vec3:
  case ElementType::Vec4: return 1;
  }
  return 0;
}

/* Every column of a matrix element starts 4-byte aligned, so a MAT3 of bytes occupies 12 and not 9.
 * Getting this wrong reads a correct file as garbage without any index going out of range, which is
 * why it is computed in one place rather than at each call site. */
size_t TightElementBytes(ElementType element, ComponentType component) {
  const size_t rows = ElementRows(element);
  const size_t columns = ElementColumns(element);
  const size_t bytes = ComponentBytes(component);
  if (columns == 1) { return rows * bytes; }
  const size_t column = ((rows * bytes) + 3) & ~size_t{3};
  return columns * column;
}

std::string MissingSemantics(const Primitive &primitive,
                             std::initializer_list<const char *> required) {
  std::string missing;
  for (const char *semantic : required) {
    if (primitive.Find(semantic) >= 0) { continue; }
    if (!missing.empty()) { missing += ", "; }
    missing += semantic;
  }
  return missing;
}

int Primitive::Find(const char *semantic) const {
  for (const Attribute &attribute : Attributes) {
    if (attribute.Semantic == semantic) { return attribute.Accessor; }
  }
  return -1;
}

} // namespace outshine::Gltf
