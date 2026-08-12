#include "Subject.h"

#include <cmath>
#include <string>

#include "Document.h"
#include "Framing.h"

namespace outshine::Gltf {

namespace {

constexpr double kPi = 3.14159265358979323846;

void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

double Length(const double v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

[[nodiscard]] bool Normalise(double v[3]) {
  const double length = Length(v);
  if (!(length > 0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

const char *ModeName(PrimitiveMode mode) {
  switch (mode) {
  case PrimitiveMode::Points: return "POINTS";
  case PrimitiveMode::Lines: return "LINES";
  case PrimitiveMode::LineLoop: return "LINE_LOOP";
  case PrimitiveMode::LineStrip: return "LINE_STRIP";
  case PrimitiveMode::Triangles: return "TRIANGLES";
  case PrimitiveMode::TriangleStrip: return "TRIANGLE_STRIP";
  case PrimitiveMode::TriangleFan: return "TRIANGLE_FAN";
  }
  return "an undeclared mode";
}

} // namespace

bool Placement::LookAt(const double eyeM[3], const double aimM[3], double rollRad, Placement &out) {
  double forward[3] = {aimM[0] - eyeM[0], aimM[1] - eyeM[1], aimM[2] - eyeM[2]};
  if (!Normalise(forward)) { return false; }
  const double worldUp[3] = {0, 1, 0};
  double right[3];
  Cross(forward, worldUp, right);
  if (!Normalise(right)) { return false; }
  double up[3];
  Cross(right, forward, up);

  const double turn = std::cos(rollRad);
  const double lean = std::sin(rollRad);
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = eyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis] * turn + up[axis] * lean;
    out.Up[axis] = up[axis] * turn - right[axis] * lean;
  }
  return true;
}

bool Placement::View(Transform &out) const {
  Transform world;
  for (int axis = 0; axis < 3; ++axis) {
    world.M[axis] = Right[axis];
    world.M[4 + axis] = Up[axis];
    world.M[8 + axis] = -Forward[axis];
    world.M[12 + axis] = EyeM[axis];
  }
  world.M[3] = world.M[7] = world.M[11] = 0;
  world.M[15] = 1;
  return world.Inverse(out);
}

bool Placement::Clip(double viewportAspect, Transform &out) const {
  Camera lens;
  lens.YfovRad = YfovRad;
  lens.ZNearM = ZNearM;
  lens.ZFarM = ZFarM;
  Transform projection, view;
  if (!lens.Projection(viewportAspect, projection)) { return false; }
  if (!View(view)) { return false; }
  out = projection * view;
  return true;
}

bool Subject::Refuse(const std::string &why) {
  Error_ = why;
  Positions_.clear();
  Indices_.clear();
  return false;
}

bool Subject::Build(const Document &document) {
  Error_.clear();
  Positions_.clear();
  Indices_.clear();

  const int sceneIndex = document.DefaultScene();
  if (sceneIndex < 0 || (size_t)sceneIndex >= document.Scenes().size()) {
    return Refuse(document.Path() + ": no default scene to draw");
  }

  /* Depth-first over the hierarchy from the scene's roots; WorldTransform already refuses a cycle
   * and a node index the file does not carry, so this walk needs no visited set of its own. */
  std::vector<int> pending(document.Scenes()[(size_t)sceneIndex].Roots.rbegin(),
                           document.Scenes()[(size_t)sceneIndex].Roots.rend());
  std::vector<double> elements;
  std::vector<uint32_t> indices;
  size_t primitives = 0;
  while (!pending.empty()) {
    const int nodeIndex = pending.back();
    pending.pop_back();
    if (nodeIndex < 0 || (size_t)nodeIndex >= document.Nodes().size()) {
      return Refuse(document.Path() + ": scene names node " + std::to_string(nodeIndex) +
                    ", which the file does not carry");
    }
    const Node &node = document.Nodes()[(size_t)nodeIndex];
    for (auto child = node.Children.rbegin(); child != node.Children.rend(); ++child) {
      pending.push_back(*child);
    }
    if (node.Mesh < 0) { continue; }
    if ((size_t)node.Mesh >= document.Meshes().size()) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) + " names mesh " +
                    std::to_string(node.Mesh) + ", which the file does not carry");
    }
    Transform world;
    if (!document.WorldTransform(nodeIndex, world)) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                    " has no world transform: " + document.Error());
    }

    for (const Primitive &primitive : document.Meshes()[(size_t)node.Mesh].Primitives) {
      ++primitives;
      if (primitive.Mode != PrimitiveMode::Triangles) {
        return Refuse(document.Path() + ": primitive of mesh " + std::to_string(node.Mesh) +
                      " is " + ModeName(primitive.Mode) +
                      ", and this subject draws TRIANGLES only");
      }
      const int position = primitive.Find("POSITION");
      if (position < 0) {
        return Refuse(document.Path() + ": primitive of mesh " + std::to_string(node.Mesh) +
                      " carries no POSITION, and nothing here invents one");
      }
      if (!document.ReadElements(position, elements)) {
        return Refuse(document.Path() + ": POSITION does not decode: " + document.Error());
      }
      if (elements.size() % 3 != 0) {
        return Refuse(document.Path() + ": POSITION decodes to " + std::to_string(elements.size()) +
                      " components, which is not a whole number of points");
      }
      const uint32_t base = (uint32_t)(Positions_.size() / 3);
      const size_t vertices = elements.size() / 3;
      for (size_t vertex = 0; vertex < vertices; ++vertex) {
        double local[3] = {elements[vertex * 3], elements[vertex * 3 + 1],
                           elements[vertex * 3 + 2]};
        double global[3];
        world.Point(local, global);
        for (int axis = 0; axis < 3; ++axis) { Positions_.push_back(global[axis]); }
      }

      if (primitive.Indices >= 0) {
        if (!document.ReadIndices(primitive.Indices, indices)) {
          return Refuse(document.Path() + ": the index accessor does not decode: " +
                        document.Error());
        }
      } else {
        indices.resize(vertices);
        for (size_t vertex = 0; vertex < vertices; ++vertex) { indices[vertex] = (uint32_t)vertex; }
      }
      if (indices.size() % 3 != 0) {
        return Refuse(document.Path() + ": " + std::to_string(indices.size()) +
                      " indices is not a whole number of triangles");
      }
      for (uint32_t index : indices) {
        if (index >= vertices) {
          return Refuse(document.Path() + ": index " + std::to_string(index) + " addresses past the " +
                        std::to_string(vertices) + " vertices of its own primitive");
        }
        Indices_.push_back(base + index);
      }
    }
  }

  if (Indices_.empty()) {
    return Refuse(document.Path() + ": the default scene draws no triangle over " +
                  std::to_string(primitives) + " primitive(s), so there is nothing to render");
  }

  for (int axis = 0; axis < 3; ++axis) {
    Min_[axis] = Max_[axis] = Positions_[(size_t)axis];
  }
  for (size_t vertex = 1; vertex < VertexCount(); ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      const double value = Positions_[vertex * 3 + (size_t)axis];
      if (value < Min_[axis]) { Min_[axis] = value; }
      if (value > Max_[axis]) { Max_[axis] = value; }
    }
  }
  return true;
}

double Subject::RadiusM() const {
  const double span[3] = {Max_[0] - Min_[0], Max_[1] - Min_[1], Max_[2] - Min_[2]};
  return 0.5 * Length(span);
}

void Subject::CentreM(double out[3]) const {
  for (int axis = 0; axis < 3; ++axis) { out[axis] = 0.5 * (Min_[axis] + Max_[axis]); }
}

bool Subject::Frame(Placement &out) const {
  const double radius = RadiusM();
  if (!(radius > 0)) { return false; }
  double centre[3];
  CentreM(centre);

  const double azimuth = kFramingAzimuthDeg * kPi / 180.0;
  const double elevation = kFramingElevationDeg * kPi / 180.0;
  /* Azimuth is measured in glTF's ground plane from +X towards +Z, elevation up from it. */
  double toEye[3] = {std::cos(elevation) * std::cos(azimuth), std::sin(elevation),
                     std::cos(elevation) * std::sin(azimuth)};

  const double yfov = 2.0 * std::atan(kFramingSensorHalfHeightMm / kFramingFocalLengthMm);
  const double distance = radius / std::sin(0.5 * yfov) / kFramingFill;
  double eye[3];
  for (int axis = 0; axis < 3; ++axis) { eye[axis] = centre[axis] + toEye[axis] * distance; }

  if (!Placement::LookAt(eye, centre, 0.0, out)) { return false; }
  out.YfovRad = yfov;
  const double floor = radius * kFramingNearFloorFraction;
  out.ZNearM = (distance - radius > floor) ? distance - radius : floor;
  out.ZFarM = distance + radius;
  return true;
}

bool Subject::DeclaredPlacement(const Document &document, Placement &out) const {
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    const int camera = document.Nodes()[node].Camera;
    if (camera < 0) { continue; }
    if ((size_t)camera >= document.Cameras().size()) { return false; }
    const Camera &lens = document.Cameras()[(size_t)camera];
    if (lens.Kind != CameraKind::Perspective) { return false; }
    Transform world;
    if (!document.WorldTransform((int)node, world)) { return false; }
    for (int axis = 0; axis < 3; ++axis) {
      out.Right[axis] = world.M[axis];
      out.Up[axis] = world.M[4 + axis];
      out.Forward[axis] = -world.M[8 + axis];
      out.EyeM[axis] = world.M[12 + axis];
    }
    if (!Normalise(out.Right) || !Normalise(out.Up) || !Normalise(out.Forward)) { return false; }
    out.YfovRad = lens.YfovRad;
    out.ZNearM = lens.ZNearM;
    out.ZFarM = lens.ZFarM;
    return true;
  }
  return false;
}

double Subject::ProjectedAreaPx(const Transform &clip, const Viewport &viewport) const {
  double total = 0;
  for (size_t triangle = 0; triangle * 3 + 2 < Indices_.size(); ++triangle) {
    double raster[3][2];
    for (int corner = 0; corner < 3; ++corner) {
      const size_t vertex = Indices_[triangle * 3 + (size_t)corner];
      const double point[3] = {Positions_[vertex * 3], Positions_[vertex * 3 + 1],
                               Positions_[vertex * 3 + 2]};
      double ndc[3];
      clip.Point(point, ndc);
      viewport.Raster(ndc, raster[corner]);
    }
    total += 0.5 * std::fabs((raster[1][0] - raster[0][0]) * (raster[2][1] - raster[0][1]) -
                             (raster[2][0] - raster[0][0]) * (raster[1][1] - raster[0][1]));
  }
  return total;
}

} // namespace outshine::Gltf
