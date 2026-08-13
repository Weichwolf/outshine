#include "Subject.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
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

bool DrawsASurface(PrimitiveMode mode) {
  return mode == PrimitiveMode::Triangles || mode == PrimitiveMode::TriangleStrip ||
         mode == PrimitiveMode::TriangleFan;
}

/* Whether `run` can be walked as this mode at all: three indices to a triangle for a list, three to
 * start a run for a strip or a fan. */
bool RunIsWhole(PrimitiveMode mode, size_t indices) {
  return (mode == PrimitiveMode::Triangles) ? (indices > 0 && indices % 3 == 0) : (indices >= 3);
}

/* WHAT A NODE'S TRANSFORM DOES TO THE ORDER OF ITS TRIANGLES. A mirroring transform turns a
 * front face into a back one, so the format restates the winding rather than the geometry
 * (Specification.adoc:1734). It is an argument of the triangulation and not a step after it, so a
 * path that produces triangles without deciding this has no spelling (`Enum.2`). */
enum class Handedness { Preserved, Reversed };

/* THE ODD TRIANGLE OF A STRIP IS (i+1, i, i+2), NOT A SLIDING WINDOW: without the swap every second
 * triangle of the run would face the other way, which is the format's rule and not a convention this
 * file chooses. A fan is (0, i, i+1) throughout and needs no such swap. Assumes `RunIsWhole`. */
void Triangulate(PrimitiveMode mode, Handedness handedness, const std::vector<uint32_t> &run,
                 std::vector<uint32_t> &out) {
  out.clear();
  if (mode == PrimitiveMode::Triangles) {
    out = run;
  } else if (mode == PrimitiveMode::TriangleStrip) {
    for (size_t at = 0; at + 2 < run.size(); ++at) {
      const size_t flipped = (at % 2 == 0) ? 0u : 1u;
      out.push_back(run[at + flipped]);
      out.push_back(run[at + 1u - flipped]);
      out.push_back(run[at + 2u]);
    }
  } else {
    for (size_t at = 1; at + 1 < run.size(); ++at) {
      out.push_back(run[0]);
      out.push_back(run[at]);
      out.push_back(run[at + 1]);
    }
  }
  if (handedness == Handedness::Reversed) {
    for (size_t triangle = 0; triangle * 3 + 2 < out.size(); ++triangle) {
      std::swap(out[triangle * 3 + 1], out[triangle * 3 + 2]);
    }
  }
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
  lens.Kind = Kind;
  lens.YfovRad = YfovRad;
  lens.XMagM = XMagM;
  lens.YMagM = YMagM;
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
  Uv_.clear();
  Normals_.clear();
  Indices_.clear();
  Parts_.clear();
  Lights_.clear();
  bool anyUv = false;
  bool anyNormal = false;

  const int sceneIndex = document.DefaultScene();
  if (sceneIndex < 0 || (size_t)sceneIndex >= document.Scenes().size()) {
    return Refuse(document.Path() + ": no default scene to draw");
  }

  /* Depth-first over the hierarchy from the scene's roots; WorldTransform already refuses a cycle
   * and a node index the file does not carry, so this walk needs no visited set of its own. */
  std::vector<int> pending(document.Scenes()[(size_t)sceneIndex].Roots.rbegin(),
                           document.Scenes()[(size_t)sceneIndex].Roots.rend());
  std::vector<double> elements;
  std::vector<uint32_t> run, indices;
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
    if (node.Light >= 0) {
      Transform placement;
      if (!document.WorldTransform(nodeIndex, placement)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and has no world transform: " + document.Error());
      }
      const LightRef &declared = document.Lights()[(size_t)node.Light];
      PlacedLight placed;
      placed.NodeName = node.Name;
      placed.LightName = declared.Name;
      placed.Light = declared.Light;
      const double origin[3] = {0, 0, 0};
      double position[3];
      placement.Point(origin, position);
      /* THE BEAM IS THE NODE'S -Z, which is `KHR_lights_punctual`'s own rule and the same convention
       * the format gives a camera. A zero-scaled node has no direction to give and is refused rather
       * than pointed somewhere. */
      const double axis[3] = {0, 0, -1};
      double beam[3];
      placement.Direction(axis, beam);
      if (!Normalise(beam)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and its transform collapses the beam to zero length");
      }
      for (int component = 0; component < 3; ++component) {
        placed.Light.Position[component] = (float)position[component];
        placed.Light.Direction[component] = (float)beam[component];
      }
      Lights_.push_back(std::move(placed));
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
      Part part;
      part.NodeName = node.Name;
      part.Material = primitive.Material;
      part.FirstVertex = VertexCount();
      part.FirstIndex = Indices_.size();
      if (!DrawsASurface(primitive.Mode)) {
        return Refuse(document.Path() + ": primitive of mesh " + std::to_string(node.Mesh) +
                      " is " + ModeName(primitive.Mode) +
                      ", and this subject draws surfaces only -- TRIANGLES, TRIANGLE_STRIP or "
                      "TRIANGLE_FAN");
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

      /* THE FIRST UV SET, PER PRIMITIVE. The run stays as long as the vertex run whatever the mix
       * is: a primitive that carried none contributes zeros there and is drawn by a pipeline with
       * no uv slot, so those zeros are unread rather than sampled at the image's corner. */
      const int uv = primitive.Find("TEXCOORD_0");
      part.HasUv = uv >= 0;
      anyUv = anyUv || part.HasUv;
      Uv_.resize((Positions_.size() / 3) * 2, 0.0);
      if (uv >= 0) {
        std::vector<double> coordinates;
        if (!document.ReadElements(uv, coordinates)) {
          return Refuse(document.Path() + ": TEXCOORD_0 does not decode: " + document.Error());
        }
        if (coordinates.size() != vertices * 2) {
          return Refuse(document.Path() + ": TEXCOORD_0 decodes to " +
                        std::to_string(coordinates.size() / 2) + " pairs over " +
                        std::to_string(vertices) + " vertices");
        }
        std::copy(coordinates.begin(), coordinates.end(),
                  Uv_.begin() + static_cast<std::ptrdiff_t>(part.FirstVertex * 2));
      }

      /* THE NORMAL, PER PRIMITIVE, ROTATED BY THE INVERSE TRANSPOSE and normalised here so that no
       * consumer has to know whether the node scaled it. The run stays as long as the vertex run
       * whatever the mix is; a primitive that carried none contributes zeros and is drawn by a
       * pipeline with no normal slot, so those zeros are unread rather than shaded as a direction. */
      const int normal = primitive.Find("NORMAL");
      part.HasNormal = normal >= 0;
      anyNormal = anyNormal || part.HasNormal;
      Normals_.resize(Positions_.size(), 0.0);
      if (normal >= 0) {
        std::vector<double> directions;
        if (!document.ReadElements(normal, directions)) {
          return Refuse(document.Path() + ": NORMAL does not decode: " + document.Error());
        }
        if (directions.size() != vertices * 3) {
          return Refuse(document.Path() + ": NORMAL decodes to " +
                        std::to_string(directions.size() / 3) + " vectors over " +
                        std::to_string(vertices) + " vertices");
        }
        for (size_t vertex = 0; vertex < vertices; ++vertex) {
          double local[3] = {directions[vertex * 3], directions[vertex * 3 + 1],
                             directions[vertex * 3 + 2]};
          double global[3];
          if (!world.Normal(local, global)) {
            return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                          " carries a NORMAL and a transform with no inverse, so the surface it is "
                          "perpendicular to has collapsed");
          }
          /* A ZERO-LENGTH NORMAL IS THE FILE'S AND IS CARRIED AS IT ARRIVED. Substituting one here
           * would make a malformed vertex look shaded; the consumer sees a zero and the picture
           * shows it. */
          (void)Normalise(global);
          for (int axis = 0; axis < 3; ++axis) {
            Normals_[(part.FirstVertex + vertex) * 3 + (size_t)axis] = global[axis];
          }
        }
      }

      if (primitive.Indices >= 0) {
        if (!document.ReadIndices(primitive.Indices, run)) {
          return Refuse(document.Path() + ": the index accessor does not decode: " +
                        document.Error());
        }
      } else {
        run.resize(vertices);
        for (size_t vertex = 0; vertex < vertices; ++vertex) { run[vertex] = (uint32_t)vertex; }
      }
      if (!RunIsWhole(primitive.Mode, run.size())) {
        return Refuse(document.Path() + ": " + std::to_string(run.size()) +
                      " indices do not make a whole run of " + ModeName(primitive.Mode));
      }
      Triangulate(primitive.Mode,
                  world.LinearDeterminant() < 0 ? Handedness::Reversed : Handedness::Preserved, run,
                  indices);
      for (uint32_t index : indices) {
        if (index >= vertices) {
          return Refuse(document.Path() + ": index " + std::to_string(index) + " addresses past the " +
                        std::to_string(vertices) + " vertices of its own primitive");
        }
        Indices_.push_back(base + index);
      }
      part.VertexCount = VertexCount() - part.FirstVertex;
      part.IndexCount = Indices_.size() - part.FirstIndex;
      /* A primitive that yielded no triangle is not a part: it is a name a per-part declaration
       * would have to answer for while nothing of it is drawn. */
      if (part.IndexCount > 0) { Parts_.push_back(part); }
    }
  }

  if (!anyUv) { Uv_.clear(); }
  if (!anyNormal) { Normals_.clear(); }
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

bool DeclaredPlacement(const Document &document, int cameraIndex, Placement &out,
                       std::string &error) {
  if (cameraIndex < 0 || (size_t)cameraIndex >= document.Cameras().size()) {
    error = document.Path() + ": camera " + std::to_string(cameraIndex) + " is asked for and the " +
            "document declares " + std::to_string(document.Cameras().size());
    return false;
  }
  size_t holder = 0;
  size_t holders = 0;
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    if (document.Nodes()[node].Camera != cameraIndex) { continue; }
    holder = node;
    ++holders;
  }
  if (holders != 1) {
    error = document.Path() + ": camera " + std::to_string(cameraIndex) + " is referenced by " +
            std::to_string(holders) + " nodes, and a placement is what exactly one node states";
    return false;
  }

  const Camera &lens = document.Cameras()[(size_t)cameraIndex];
  Transform world;
  if (!document.WorldTransform((int)holder, world)) {
    error = document.Path() + ": node " + std::to_string(holder) + " carries camera " +
            std::to_string(cameraIndex) + " and its world transform does not resolve";
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) {
    out.Right[axis] = world.M[axis];
    out.Up[axis] = world.M[4 + axis];
    out.Forward[axis] = -world.M[8 + axis];
    out.EyeM[axis] = world.M[12 + axis];
  }
  if (!Normalise(out.Right) || !Normalise(out.Up) || !Normalise(out.Forward)) {
    error = document.Path() + ": node " + std::to_string(holder) + " carries camera " +
            std::to_string(cameraIndex) + " and its basis has collapsed";
    return false;
  }
  out.Kind = lens.Kind;
  out.YfovRad = lens.YfovRad;
  out.XMagM = lens.XMagM;
  out.YMagM = lens.YMagM;
  out.ZNearM = lens.ZNearM;
  out.ZFarM = lens.ZFarM;
  return true;
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
