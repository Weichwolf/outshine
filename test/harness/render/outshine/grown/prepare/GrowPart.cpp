/* THE GENERATOR, RUN, SO THAT WHAT IT GREW CAN BE SCORED. The preparer needs one `.glb` per case
 * before Blender ever opens; a generated part's bytes therefore have to exist offline, and the only
 * thing that may produce them is the engine's own C++ -- a second implementation in the preparer's
 * Python would score a tree this repository does not draw. So the preparer builds this and runs it,
 * exactly as it locates and runs Blender (test/harness/shared/corpus/README.md).
 *
 * IT IS NOT A TEST AND CARRIES NO VERDICT. It grows, writes and reports; what the part is worth is
 * decided by the render case, and what the emit path guarantees is held by
 * test/unit/gltf/AProducedSubjectIsTheOneItStated.
 *
 * THE METRE ENTERS HERE AND NOWHERE ELSE. `TreeMesh` comes back normalised to height 1 -- the one
 * number carrying a metre is `TreeSpecies::HeightM` -- and glTF's frame is metres, so the scale is
 * applied at the handover and the file says how tall the tree is. */
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
  /* The model length of one pixel as a fraction of the tree's own height, which is the budget
   * TreeMesher::Draw already takes. 0 draws the plant entire. */
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

/* THE DECLARED COLOUR IS sRGB AND glTF's `baseColorFactor` IS LINEAR. Every colour in a species file
 * is sRGB and says so (src/assets/world/species/beech.json, `bark_origin`); writing it into the file
 * unconverted is the two-to-four-times-too-bright bark that note was written about. */
double LinearFromSrgb(double encoded) {
  return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

/* SIX TIMES THE SIGNED VOLUME THE TRIANGLES ENCLOSE. It is the winding, measured rather than
 * assumed: glTF's front face is counter-clockwise seen from outside, so a closed mesh wound the
 * format's way encloses a POSITIVE volume, and one wound inside out renders as nothing but back
 * faces -- 46 101 black pixels of a sphere is what that cost the corpus once already. */
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

/* THE FRAMING RULE'S ANSWER FOR THIS SUBJECT, in the shape a manifest declares a camera in, so a
 * case's camera is transcribed from the rule rather than composed beside it. */
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

} // namespace

int main(int argc, char **argv) {
  Request request;
  std::string error;
  if (!Read(argc, argv, request, error)) { return Fail(error); }

  outshine::Generators::TreeSpecies species;
  if (!outshine::Clients::ReadSpecies(request.SpeciesPath.c_str(), &species)) {
    return Fail("the species declaration at " + request.SpeciesPath + " does not read: " +
                species.Error());
  }

  /* GROWN ONCE, DRAWN AT THE ASKED-FOR BUDGET. The skeleton is what the species declares and the
   * budget is a view over it, so this program's part at one budget is a subset of its part at a
   * finer one rather than another tree. */
  outshine::Generators::TreeGrower grower;
  outshine::Generators::TreeMesher mesher;
  outshine::Generators::TreeSkeleton plant;
  outshine::Generators::TreeMesh mesh;
  grower.Grow(species, plant);
  mesher.Draw(plant, (float)request.PixelHeightFrac, mesh);
  if (mesh.BarkIdx.empty()) {
    return Fail("the grower produced no triangle for " + species.Name() + " at the requested budget");
  }

  /* The mesh is interleaved position and normal (`core/ChunkVtx.h`, `PlainVtx`) and a piece states
   * planar runs, so the two are separated once here -- at the handover, off the frame path. */
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
