#include "Tangents.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <string>

namespace outshine::Gltf {

namespace {

[[nodiscard]] bool NotZero(double value) {
  return std::fabs(value) > std::numeric_limits<double>::min();
}

struct Vector {
  double X = 0, Y = 0, Z = 0;

  Vector operator+(const Vector &other) const { return {X + other.X, Y + other.Y, Z + other.Z}; }

  Vector operator-(const Vector &other) const { return {X - other.X, Y - other.Y, Z - other.Z}; }

  Vector operator*(double scale) const { return {X * scale, Y * scale, Z * scale}; }

  bool operator==(const Vector &other) const {
    return X == other.X && Y == other.Y && Z == other.Z;
  }
};

[[nodiscard]] double Dot(const Vector &a, const Vector &b) {
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

[[nodiscard]] double Length(const Vector &v) {
  return std::sqrt(Dot(v, v));
}

[[nodiscard]] bool NotZero(const Vector &v) {
  return NotZero(v.X) || NotZero(v.Y) || NotZero(v.Z);
}

[[nodiscard]] Vector Normalised(const Vector &v) {
  return v * (1.0 / Length(v));
}

[[nodiscard]] Vector Perpendicular(const Vector &v, const Vector &n) {
  const Vector flat = v - n * Dot(n, v);
  return NotZero(flat) ? Normalised(flat) : flat;
}

struct TriangleInfo {
  Vector Os, Ot;
  double MagS = 0, MagT = 0;
  int Neighbour[3] = {-1, -1, -1};
  int Group[3] = {-1, -1, -1};

  size_t FirstCorner = 0;
  bool OrientPreserving = false;
  bool GroupsWithAny = true;
  bool Degenerate = false;
};

struct Space {
  Vector Os{1, 0, 0}, Ot{0, 1, 0};
  double MagS = 1, MagT = 1;
  int Counter = 0;
  bool Orient = false;
};

[[nodiscard]] Space Averaged(const Space &first, const Space &second) {
  Space result;
  if (first.MagS == second.MagS && first.MagT == second.MagT && first.Os == second.Os &&
      first.Ot == second.Ot) {
    result.Os = first.Os;
    result.Ot = first.Ot;
    result.MagS = first.MagS;
    result.MagT = first.MagT;
    return result;
  }
  result.MagS = 0.5 * (first.MagS + second.MagS);
  result.MagT = 0.5 * (first.MagT + second.MagT);
  result.Os = first.Os + second.Os;
  result.Ot = first.Ot + second.Ot;
  if (NotZero(result.Os)) { result.Os = Normalised(result.Os); }
  if (NotZero(result.Ot)) { result.Ot = Normalised(result.Ot); }
  return result;
}

struct Group {
  size_t Vertex = 0;
  bool OrientPreserving = false;
  std::vector<int> Faces;
};

class Basis {
public:
  Basis(const TangentSubject &subject, size_t triangles)
      : Subject_(subject), Corner_(triangles * 3), Welded_(triangles * 3), Triangles_(triangles) {
    for (size_t corner = 0; corner < Corner_.size(); ++corner) { Corner_[corner] = corner; }
  }

  void Weld();
  void MarkDegenerate();
  void MoveDegenerateLast();
  void Measure();
  void MatchEdges();
  void BuildGroups();
  void FillSpaces();
  void CopyIntoDegenerate();
  void Emit(std::vector<double> &out) const;

private:
  [[nodiscard]] Vector PositionOf(size_t corner) const {
    return At(Subject_.PositionsM, corner, 3);
  }

  [[nodiscard]] Vector NormalOf(size_t corner) const { return At(Subject_.Normals, corner, 3); }

  [[nodiscard]] Vector TexCoordOf(size_t corner) const {
    const size_t vertex = Subject_.Indices[corner];
    return {Subject_.Uv[vertex * 2], -Subject_.Uv[vertex * 2 + 1], 0.0};
  }

  [[nodiscard]] Vector At(const double *run, size_t corner, size_t width) const {
    const size_t vertex = Subject_.Indices[corner];
    return {run[vertex * width], run[vertex * width + 1], run[vertex * width + 2]};
  }

  [[nodiscard]] Space Evaluate(const std::vector<int> &faces, size_t vertex) const;
  void Reach(int face, int group);

  const TangentSubject &Subject_;

  std::vector<size_t> Corner_;

  std::vector<size_t> Welded_;
  std::vector<TriangleInfo> Triangles_;
  std::vector<Group> Groups_;
  std::vector<Space> Spaces_;
  size_t Healthy_ = 0;
};

struct AttributeKey {
  uint64_t Bits[8] = {};

  [[nodiscard]] bool operator<(const AttributeKey &other) const {
    return std::memcmp(Bits, other.Bits, sizeof Bits) < 0;
  }
};

[[nodiscard]] uint64_t BitsOf(double value) {
  const double folded = value == 0.0 ? 0.0 : value;
  uint64_t bits = 0;
  std::memcpy(&bits, &folded, sizeof bits);
  return bits;
}

void Basis::Weld() {
  std::map<AttributeKey, size_t> seen;
  for (size_t corner = 0; corner < Corner_.size(); ++corner) {
    const Vector position = PositionOf(Corner_[corner]);
    const Vector normal = NormalOf(Corner_[corner]);
    const Vector texture = TexCoordOf(Corner_[corner]);
    AttributeKey key;
    const double components[8] = {
        position.X, position.Y, position.Z, normal.X, normal.Y, normal.Z, texture.X, texture.Y};
    for (size_t at = 0; at < 8; ++at) { key.Bits[at] = BitsOf(components[at]); }
    const auto found = seen.find(key);
    if (found != seen.end()) {
      Welded_[corner] = found->second;
    } else {
      Welded_[corner] = corner;
      seen.emplace(key, corner);
    }
  }
}

void Basis::MarkDegenerate() {
  for (size_t triangle = 0; triangle < Triangles_.size(); ++triangle) {
    const Vector a = PositionOf(Corner_[triangle * 3]);
    const Vector b = PositionOf(Corner_[triangle * 3 + 1]);
    const Vector c = PositionOf(Corner_[triangle * 3 + 2]);
    Triangles_[triangle].Degenerate = a == b || a == c || b == c;
  }
}

void Basis::MoveDegenerateLast() {
  std::vector<size_t> order(Triangles_.size());
  for (size_t at = 0; at < order.size(); ++at) { order[at] = at; }
  std::stable_partition(order.begin(), order.end(), [this](size_t triangle) {
    return !Triangles_[triangle].Degenerate;
  });
  std::vector<size_t> corners(Corner_.size());
  std::vector<TriangleInfo> triangles(Triangles_.size());
  Healthy_ = 0;
  for (size_t at = 0; at < order.size(); ++at) {
    triangles[at] = Triangles_[order[at]];
    triangles[at].FirstCorner = order[at] * 3;
    for (size_t corner = 0; corner < 3; ++corner) {
      corners[at * 3 + corner] = Corner_[order[at] * 3 + corner];
    }
    if (!triangles[at].Degenerate) { ++Healthy_; }
  }
  Corner_ = std::move(corners);
  Triangles_ = std::move(triangles);
}

void Basis::Measure() {
  for (size_t triangle = 0; triangle < Healthy_; ++triangle) {
    TriangleInfo &info = Triangles_[triangle];
    const Vector v1 = PositionOf(Corner_[triangle * 3]);
    const Vector v2 = PositionOf(Corner_[triangle * 3 + 1]);
    const Vector v3 = PositionOf(Corner_[triangle * 3 + 2]);
    const Vector t1 = TexCoordOf(Corner_[triangle * 3]);
    const Vector t2 = TexCoordOf(Corner_[triangle * 3 + 1]);
    const Vector t3 = TexCoordOf(Corner_[triangle * 3 + 2]);
    const double t21x = t2.X - t1.X, t21y = t2.Y - t1.Y;
    const double t31x = t3.X - t1.X, t31y = t3.Y - t1.Y;
    const Vector d1 = v2 - v1, d2 = v3 - v1;
    const double signedArea = t21x * t31y - t21y * t31x;
    const Vector os = d1 * t31y - d2 * t21y;
    const Vector ot = d1 * -t31x + d2 * t21x;
    info.OrientPreserving = signedArea > 0;
    if (!NotZero(signedArea)) { continue; }
    const double area = std::fabs(signedArea);
    const double lengthS = Length(os), lengthT = Length(ot);
    const double sign = info.OrientPreserving ? 1.0 : -1.0;
    if (NotZero(lengthS)) { info.Os = os * (sign / lengthS); }
    if (NotZero(lengthT)) { info.Ot = ot * (sign / lengthT); }
    info.MagS = lengthS / area;
    info.MagT = lengthT / area;
    info.GroupsWithAny = !(NotZero(info.MagS) && NotZero(info.MagT));
  }
}

void Basis::MatchEdges() {
  std::map<std::pair<size_t, size_t>, std::pair<int, int>> open;
  for (size_t triangle = 0; triangle < Healthy_; ++triangle) {
    for (size_t corner = 0; corner < 3; ++corner) {
      const size_t from = Welded_[Corner_[triangle * 3 + corner]];
      const size_t to = Welded_[Corner_[triangle * 3 + (corner < 2 ? corner + 1 : 0)]];
      const auto reversed = open.find({to, from});
      if (reversed != open.end()) {
        Triangles_[triangle].Neighbour[corner] = reversed->second.first;
        Triangles_[static_cast<size_t>(reversed->second.first)]
            .Neighbour[static_cast<size_t>(reversed->second.second)] = static_cast<int>(triangle);
        open.erase(reversed);
        continue;
      }
      open.emplace(std::pair<size_t, size_t>{from, to},
                   std::pair<int, int>{static_cast<int>(triangle), static_cast<int>(corner)});
    }
  }
}

void Basis::Reach(int start, int group) {
  std::vector<int> pending{start};
  while (!pending.empty()) {
    const int face = pending.back();
    pending.pop_back();
    TriangleInfo &info = Triangles_[static_cast<size_t>(face)];
    size_t corner = 3;
    for (size_t at = 0; at < 3; ++at) {
      if (Welded_[Corner_[static_cast<size_t>(face) * 3 + at]] ==
          Groups_[static_cast<size_t>(group)].Vertex) {
        corner = at;
      }
    }
    if (corner == 3) { continue; }
    if (info.Group[corner] >= 0) { continue; }
    if (info.GroupsWithAny && info.Group[0] < 0 && info.Group[1] < 0 && info.Group[2] < 0) {
      info.OrientPreserving = Groups_[static_cast<size_t>(group)].OrientPreserving;
    }
    if (info.OrientPreserving != Groups_[static_cast<size_t>(group)].OrientPreserving) { continue; }
    Groups_[static_cast<size_t>(group)].Faces.push_back(face);
    info.Group[corner] = group;
    const int right = info.Neighbour[corner > 0 ? corner - 1 : 2];
    const int left = info.Neighbour[corner];
    if (right >= 0) { pending.push_back(right); }
    if (left >= 0) { pending.push_back(left); }
  }
}

void Basis::BuildGroups() {
  for (size_t triangle = 0; triangle < Healthy_; ++triangle) {
    for (size_t corner = 0; corner < 3; ++corner) {
      if (Triangles_[triangle].GroupsWithAny || Triangles_[triangle].Group[corner] >= 0) {
        continue;
      }
      Group group;
      group.Vertex = Welded_[Corner_[triangle * 3 + corner]];
      group.OrientPreserving = Triangles_[triangle].OrientPreserving;
      Groups_.push_back(std::move(group));
      Reach(static_cast<int>(triangle), static_cast<int>(Groups_.size()) - 1);
    }
  }
}

Space Basis::Evaluate(const std::vector<int> &faces, size_t vertex) const {
  Space result;
  result.Os = {0, 0, 0};
  result.Ot = {0, 0, 0};
  result.MagS = 0;
  result.MagT = 0;
  double angles = 0;
  for (const int face : faces) {
    const TriangleInfo &info = Triangles_[static_cast<size_t>(face)];
    if (info.GroupsWithAny) { continue; }
    size_t corner = 3;
    for (size_t at = 0; at < 3; ++at) {
      if (Welded_[Corner_[static_cast<size_t>(face) * 3 + at]] == vertex) { corner = at; }
    }
    if (corner == 3) { continue; }
    const Vector normal = NormalOf(Corner_[static_cast<size_t>(face) * 3 + corner]);
    const Vector os = Perpendicular(info.Os, normal);
    const Vector ot = Perpendicular(info.Ot, normal);
    const Vector here = PositionOf(Corner_[static_cast<size_t>(face) * 3 + corner]);
    const Vector before =
        PositionOf(Corner_[static_cast<size_t>(face) * 3 + (corner > 0 ? corner - 1 : 2)]);
    const Vector after =
        PositionOf(Corner_[static_cast<size_t>(face) * 3 + (corner < 2 ? corner + 1 : 0)]);
    const Vector first = Perpendicular(before - here, normal);
    const Vector second = Perpendicular(after - here, normal);
    const double cosine = std::min(1.0, std::max(-1.0, Dot(first, second)));
    const double angle = std::acos(cosine);
    result.Os = result.Os + os * angle;
    result.Ot = result.Ot + ot * angle;
    result.MagS += angle * info.MagS;
    result.MagT += angle * info.MagT;
    angles += angle;
  }
  if (NotZero(result.Os)) { result.Os = Normalised(result.Os); }
  if (NotZero(result.Ot)) { result.Ot = Normalised(result.Ot); }
  if (angles > 0) {
    result.MagS /= angles;
    result.MagT /= angles;
  }
  return result;
}

void Basis::FillSpaces() {
  constexpr double kDefaultThresholdCosine = -1.0;
  Spaces_.assign(Corner_.size(), Space{});
  for (const Group &group : Groups_) {
    std::vector<std::vector<int>> subgroups;
    std::vector<Space> spaces;
    for (const int face : group.Faces) {
      const TriangleInfo &info = Triangles_[static_cast<size_t>(face)];
      size_t corner = 3;
      for (size_t at = 0; at < 3; ++at) {
        if (info.Group[at] == static_cast<int>(&group - Groups_.data())) { corner = at; }
      }
      if (corner == 3) { continue; }
      const Vector normal = NormalOf(Corner_[static_cast<size_t>(face) * 3 + corner]);
      const Vector os = Perpendicular(info.Os, normal);
      const Vector ot = Perpendicular(info.Ot, normal);
      std::vector<int> members;
      for (const int other : group.Faces) {
        const TriangleInfo &sibling = Triangles_[static_cast<size_t>(other)];
        const bool any = info.GroupsWithAny || sibling.GroupsWithAny;
        const Vector os2 = Perpendicular(sibling.Os, normal);
        const Vector ot2 = Perpendicular(sibling.Ot, normal);
        if (any || other == face ||
            (Dot(os, os2) > kDefaultThresholdCosine && Dot(ot, ot2) > kDefaultThresholdCosine)) {
          members.push_back(other);
        }
      }
      std::sort(members.begin(), members.end());
      size_t which = 0;
      while (which < subgroups.size() && subgroups[which] != members) { ++which; }
      if (which == subgroups.size()) {
        subgroups.push_back(members);
        spaces.push_back(Evaluate(members, group.Vertex));
      }
      Space &out = Spaces_[Triangles_[static_cast<size_t>(face)].FirstCorner + corner];
      out = out.Counter == 1 ? Averaged(out, spaces[which]) : spaces[which];
      out.Counter += 1;
      out.Orient = group.OrientPreserving;
    }
  }
}

void Basis::CopyIntoDegenerate() {
  for (size_t triangle = Healthy_; triangle < Triangles_.size(); ++triangle) {
    for (size_t corner = 0; corner < 3; ++corner) {
      const size_t wanted = Welded_[Corner_[triangle * 3 + corner]];
      for (size_t healthy = 0; healthy < Healthy_ * 3; ++healthy) {
        if (Welded_[Corner_[healthy]] != wanted) { continue; }
        Spaces_[Triangles_[triangle].FirstCorner + corner] =
            Spaces_[Triangles_[healthy / 3].FirstCorner + healthy % 3];
        break;
      }
    }
  }
}

void Basis::Emit(std::vector<double> &out) const {
  out.assign(Corner_.size() * 4, 0.0);
  for (size_t corner = 0; corner < Spaces_.size(); ++corner) {
    const Space &space = Spaces_[corner];
    out[corner * 4] = space.Os.X;
    out[corner * 4 + 1] = space.Os.Y;
    out[corner * 4 + 2] = space.Os.Z;
    out[corner * 4 + 3] = space.Orient ? 1.0 : -1.0;
  }
}

} // namespace

bool GenerateTangents(const TangentSubject &subject, std::vector<double> &out, std::string &error) {
  out.clear();
  if (!subject.PositionsM || !subject.Normals || !subject.Uv || !subject.Indices) {
    error = "a tangent basis needs positions, normals, texture coordinates and indices, and one of "
            "the four was not handed over";
    return false;
  }
  if (subject.IndexCount == 0 || subject.IndexCount % 3 != 0) {
    error = "a tangent basis is generated over triangles and " +
            std::to_string(subject.IndexCount) + " indices are not a whole number of them";
    return false;
  }
  for (size_t at = 0; at < subject.IndexCount; ++at) {
    if (subject.Indices[at] >= subject.VertexCount) {
      error = "index " + std::to_string(subject.Indices[at]) + " addresses past the " +
              std::to_string(subject.VertexCount) + " vertices the basis is generated over";
      return false;
    }
  }

  Basis basis(subject, subject.IndexCount / 3);
  basis.Weld();
  basis.MarkDegenerate();
  basis.MoveDegenerateLast();
  basis.Measure();
  basis.MatchEdges();
  basis.BuildGroups();
  basis.FillSpaces();
  basis.CopyIntoDegenerate();
  basis.Emit(out);
  return true;
}

} // namespace outshine::Gltf
