#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Species.h"

#include "Emit.h"
#include "Subject.h"

#include "TreeGrower.h"
#include "TreeMesh.h"
#include "TreeMesher.h"
#include "TreeSkeleton.h"
#include "TreeSpecies.h"

namespace {

using outshine::Span;

struct Request {
  std::string SpeciesPath;
  std::string OutputPath;
  std::string NodeName;

  double PixelHeightFrac = 0.0;
};

[[nodiscard]] bool Read(int argc, char **argv, Request &out, std::string &error) {
  for (int at = 1; at < argc; ++at) {
    const std::string flag = argv[at];
    const bool last = at + 1 >= argc;
    if (last) {
      error = flag + " wants a value";
      return false;
    }
    const std::string value = argv[++at];
    if (flag == "--species") {
      out.SpeciesPath = value;
    } else if (flag == "--out") {
      out.OutputPath = value;
    } else if (flag == "--node") {
      out.NodeName = value;
    } else if (flag == "--pixel-height-frac") {
      out.PixelHeightFrac = std::atof(value.c_str());
    } else {
      error = "unknown argument " + flag;
      return false;
    }
  }
  if (out.SpeciesPath.empty() || out.OutputPath.empty() || out.NodeName.empty()) {
    error = "--species, --node and --out are all required";
    return false;
  }
  if (!(out.PixelHeightFrac >= 0.0)) {
    error = "--pixel-height-frac is a fraction of the tree's height and cannot be negative";
    return false;
  }
  return true;
}

double LinearFromSrgb(double encoded) {
  return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

double SixSignedVolume(const std::vector<float> &positions, const std::vector<uint32_t> &indices) {
  double total = 0.0;
  for (size_t corner = 0; corner + 2 < indices.size(); corner += 3) {
    const float *a = &positions[(size_t)indices[corner] * 3];
    const float *b = &positions[(size_t)indices[corner + 1] * 3];
    const float *c = &positions[(size_t)indices[corner + 2] * 3];
    total += (double)a[0] * ((double)b[1] * c[2] - (double)b[2] * c[1]) -
             (double)a[1] * ((double)b[0] * c[2] - (double)b[2] * c[0]) +
             (double)a[2] * ((double)b[0] * c[1] - (double)b[1] * c[0]);
  }
  return total;
}

void PrintCamera(const outshine::Gltf::Placement &eye, const double aim[3]) {
  std::printf("  \"framingCamera\": {\n");
  std::printf("    \"source\": \"manifest\",\n");
  std::printf("    \"positionM\": [%.17g, %.17g, %.17g],\n", eye.EyeM[0], eye.EyeM[1], eye.EyeM[2]);
  std::printf("    \"lookAtM\": [%.17g, %.17g, %.17g],\n", aim[0], aim[1], aim[2]);
  std::printf("    \"rollRad\": 0.0,\n");
  std::printf("    \"yfovRad\": %.17g,\n", eye.YfovRad);
  std::printf("    \"sensorHeightMm\": 24.0,\n");
  std::printf("    \"clipStartM\": %.17g,\n", eye.ZNearM);
  std::printf("    \"clipEndM\": %.17g\n", eye.ZFarM);
  std::printf("  },\n");
}

int Fail(const std::string &why) {
  std::fprintf(stderr, "GrowPart: %s\n", why.c_str());
  return 1;
}

}

int main(int argc, char **argv) {
  Request request;
  std::string error;
  if (!Read(argc, argv, request, error)) { return Fail(error); }

  outshine::Generators::TreeSpecies species;
  if (!outshine::Clients::ReadSpecies(request.SpeciesPath.c_str(), &species)) {
    return Fail("the species declaration at " + request.SpeciesPath + " does not read: " +
                species.Error());
  }

  outshine::Generators::TreeGrower grower;
  outshine::Generators::TreeMesher mesher;
  outshine::Generators::TreeSkeleton plant;
  outshine::Generators::TreeMesh mesh;
  grower.Grow(species, plant);
  mesher.Draw(plant, (float)request.PixelHeightFrac, mesh);
  if (mesh.BarkIdx.empty()) {
    return Fail("the grower produced no triangle for " + species.Name() + " at the requested budget");
  }

  const size_t vertices = mesh.BarkVertexCount();
  const double heightM = (double)species.HeightM();
  std::vector<float> positions(vertices * 3);
  std::vector<float> normals(vertices * 3);
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const float *source = &mesh.BarkVerts[vertex * (size_t)outshine::Generators::TreeMesh::kBarkFloats];
    for (size_t axis = 0; axis < 3; ++axis) {
      positions[vertex * 3 + axis] = (float)((double)source[axis] * heightM);
      normals[vertex * 3 + axis] = source[3 + axis];
    }
  }

  outshine::Gltf::Piece bark;
  bark.NodeName = request.NodeName;
  bark.Material = 0;
  bark.PositionsM = Span<const float>(positions.data(), positions.size());
  bark.Normals = Span<const float>(normals.data(), normals.size());
  bark.Indices = Span<const uint32_t>(mesh.BarkIdx.data(), mesh.BarkIdx.size());

  outshine::Gltf::Assembly assembly;
  assembly.Pieces = Span<const outshine::Gltf::Piece>(&bark, 1);
  outshine::Gltf::Subject subject;
  if (!subject.Assemble(assembly)) { return Fail(subject.Error()); }

  outshine::Gltf::MaterialRef surface;
  surface.Name = species.Name() + " bark";
  for (int channel = 0; channel < 3; ++channel) {
    surface.Surface.BaseColour[channel] =
        (float)LinearFromSrgb((double)species.ShadingParams().BarkColor[channel]);
  }
  surface.Surface.Roughness = 1.0f;
  surface.Surface.Metalness = 0.0f;
  surface.Surface.DoubleSided = false;

  outshine::Gltf::Emission what;
  what.Geometry = &subject;
  what.Materials = Span<const outshine::Gltf::MaterialRef>(&surface, 1);
  what.Generator = "outshine TreeGrower";
  std::vector<uint8_t> glb;
  if (!outshine::Gltf::Emit(what, glb, error)) { return Fail(error); }

  FILE *out = fopen(request.OutputPath.c_str(), "wb");
  if (!out) { return Fail("cannot write " + request.OutputPath); }
  const size_t written = fwrite(glb.data(), 1, glb.size(), out);
  fclose(out);
  if (written != glb.size()) { return Fail("short write to " + request.OutputPath); }

  outshine::Gltf::Placement framing;
  const bool framed = subject.Frame(framing);
  double centre[3];
  subject.CentreM(centre);
  std::printf("{\n");
  std::printf("  \"species\": \"%s\",\n", species.Name().c_str());
  std::printf("  \"botanical\": \"%s\",\n", species.Botanical().c_str());
  std::printf("  \"node\": \"%s\",\n", request.NodeName.c_str());
  std::printf("  \"pixelHeightFrac\": %.17g,\n", request.PixelHeightFrac);
  std::printf("  \"heightM\": %.17g,\n", heightM);
  std::printf("  \"spreadM\": %.17g,\n", (double)species.SpreadM());
  std::printf("  \"vertices\": %zu,\n", subject.VertexCount());
  std::printf("  \"triangles\": %zu,\n", subject.TriangleCount());
  std::printf("  \"leafPoints\": %zu,\n", plant.LeafPoints.size());
  std::printf("  \"growPasses\": %d,\n", grower.Passes());
  std::printf("  \"dbhErrorRel\": %.17g,\n", (double)grower.DbhErrorRel());
  std::printf("  \"minM\": [%.17g, %.17g, %.17g],\n", subject.MinM()[0], subject.MinM()[1],
              subject.MinM()[2]);
  std::printf("  \"maxM\": [%.17g, %.17g, %.17g],\n", subject.MaxM()[0], subject.MaxM()[1],
              subject.MaxM()[2]);
  std::printf("  \"centreM\": [%.17g, %.17g, %.17g],\n", centre[0], centre[1], centre[2]);
  std::printf("  \"radiusM\": %.17g,\n", subject.RadiusM());
  std::printf("  \"sixSignedVolumeM3\": %.17g,\n", SixSignedVolume(positions, mesh.BarkIdx));
  if (framed) { PrintCamera(framing, centre); }
  std::printf("  \"bytes\": %zu\n", glb.size());
  std::printf("}\n");
  return 0;
}
