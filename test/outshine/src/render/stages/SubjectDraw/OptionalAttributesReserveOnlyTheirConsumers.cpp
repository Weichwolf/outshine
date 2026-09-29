#include "Check.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "SubjectDraw.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const OwnedDevice device(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        false,
        nullptr));
    CHECK(device, "GPU device opens");
    if (!device) { return Report(); }
    constexpr std::array<float, 9> positions{-1, -1, 0, 1, -1, 0, 0, 1, 0};
    constexpr std::array<float, 9> normals{0, 0, 1, 0, 0, 1, 0, 0, 1};
    constexpr std::array<float, 9> emitted{};
    constexpr std::array<float, 6> uv{0, 0, 1, 0, 0, 1};
    constexpr std::array<float, 12> tangents{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};
    constexpr std::array<uint32_t, 3> indices{0, 1, 2};
    const std::array<StoredVertex, 3> vertices{
        StoredVertex::Of({{-1, -1, 0}}, {{0, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{1, -1, 0}}, {{1, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
    for (const bool mapped : {false, true}) {
      for (const bool second : {false, true}) {
        SubjectDraw draw;
        std::string error;
        Gpu gpu;
        gpu.Device = device.Get();
        gpu.HdrFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        gpu.SurfaceFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        gpu.Width = gpu.Height = 32;
        CHECK(gpu.SceneColours.Add(Resource::SceneLinear), "scene attachment adds");
        CHECK(draw.Configure(gpu, error), "native pipelines configure");
        const std::array<SubjectMaterial, 1> materials{};
        CHECK(draw.AppendMaterials(materials, error), "material registers");
        constexpr std::array<uint32_t, 1> slots{0};
        draw.SetNativePieceSurfaces(slots);
        CHECK(draw.PlacePiece(PieceMesh{.Verts = vertices, .Indices = indices}, error) != kNoPiece,
              "independent piece precedes the main mesh in the shared arena");
        DrawList draws;
        const auto layout =
            mapped ? (second ? VertexLayout::PositionNormalUvUv1Tangent
                             : VertexLayout::PositionNormalUvTangent)
                   : (second ? VertexLayout::PositionNormalUvUv1 : VertexLayout::PositionNormalUv);
        CHECK(draws.Add(DrawItem{.IndexCount = 3, .Layout = layout}, error), "draw adds");
        draws.Compile();
        SubjectMesh mesh;
        mesh.Verts.From = positions.data();
        mesh.Positions = positions;
        mesh.Emitted.From = emitted.data();
        mesh.Normals.From = normals.data();
        mesh.Uv.From = uv.data();
        if (mapped) { mesh.Tangents.From = tangents.data(); }
        if (second) { mesh.Uv1.From = uv.data(); }
        mesh.VertexCount = 3;
        mesh.Indices = indices.data();
        mesh.IndexCount = 3;
        mesh.Draws = &draws;
        const auto ticket = draw.BeginMesh(mesh);
        CHECK(ticket.has_value(), "main mesh admitted after the independent piece");
        if (!ticket) { continue; }
        CHECK(draw.FinishMesh(*ticket, mesh, error), "main mesh uploads complete");
        const auto &resident = draw.Resident();
        using Stream = SubjectResidency::Stream;
        CHECK((resident.HeldOf(Stream::Tangent) != 0) == mapped,
              "only a tangent consumer reserves the tangent stream");
        CHECK((resident.HeldOf(Stream::Uv1) != 0) == second,
              "only a second-UV consumer reserves the second-UV stream");
        const auto verify = [&](Stream stream, const auto &expected, uint32_t first) {
          const auto offset = first * static_cast<uint32_t>(sizeof(expected) / 3);
          Readback readback;
          return readback.FromBuffer(device.Get(),
                                     resident.Buffer(stream).Get(),
                                     offset + sizeof(expected)) == ReadState::Ready &&
                 std::memcmp(readback.Rows() + offset, expected.data(), sizeof(expected)) == 0;
        };
        if (mapped) {
          CHECK(verify(Stream::Tangent, tangents, resident.SubjectVertices().First),
                "main tangents reach their independently addressed GPU range");
        }
        if (second) {
          CHECK(verify(Stream::Uv1, uv, resident.SubjectVertices().First),
                "main second UVs reach their independently addressed GPU range");
        }
        const auto pieceFirst = resident.VertexRoom();
        CHECK(draw.PlacePiece(PieceMesh{.Tangents = tangents,
                                        .Verts = vertices,
                                        .Indices = indices,
                                        .Textured = true},
                              error) != kNoPiece,
              "independent tangent-bearing piece reserves and uploads its own range");
        const SubjectMesh empty;
        const auto cleared = draw.BeginMesh(empty);
        CHECK(cleared.has_value(), "main mesh clears without retiring independent pieces");
        CHECK(draw.PiecesStanding() == 2 && verify(Stream::Tangent, tangents, pieceFirst),
              "independent tangent data survives removal of the main mesh");
      }
    }
  }
  SDL_Quit();
  return Report();
}
