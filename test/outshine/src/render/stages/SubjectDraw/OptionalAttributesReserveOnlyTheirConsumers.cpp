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
    constexpr std::array<float, 9> previous{-1, -1, 0, 1, -1, 0, 0, 0.5f, 0};
    constexpr std::array<float, 9> normals{0, 0, 1, 0, 0, 1, 0, 0, 1};
    constexpr std::array<float, 9> emitted{};
    constexpr std::array<float, 6> uv{0, 0, 1, 0, 0, 1};
    constexpr std::array<float, 12> tangents{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};
    constexpr std::array<float, 12> colours{1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
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
        CHECK(gpu.SceneColours.Add(Resource::SceneVelocity), "motion attachment adds");
        CHECK(draw.Configure(gpu, error), "native pipelines configure");
        const std::array<SubjectMaterial, 1> materials{};
        CHECK(draw.AppendMaterials(materials, error), "material registers");
        constexpr std::array<uint32_t, 1> slots{0};
        draw.SetNativePieceSurfaces(slots);
        CHECK(draw.PlacePiece(PieceMesh{.Verts = vertices, .Indices = indices}, error) != kNoPiece,
              "independent piece precedes the main mesh in the shared arena");
        const auto &resident = draw.Resident();
        using Stream = SubjectResidency::Stream;
        CHECK(resident.HeldOf(Stream::Previous) == 0,
              "rigid pieces require no duplicate previous-position buffer");
        CHECK(resident.HeldOf(Stream::Uv) == 0 && resident.HeldOf(Stream::Colour) == 0,
              "untextured untinted pieces reserve neither optional stream");
        DrawList draws;
        const auto layout = mapped ? (second ? VertexLayout::PositionNormalUvUv1TangentColour
                                             : VertexLayout::PositionNormalUvTangent)
                                   : (second ? VertexLayout::PositionNormalUvUv1Colour
                                             : VertexLayout::PositionNormalUv);
        CHECK(draws.Add(DrawItem{.IndexCount = 3, .Layout = layout}, error), "draw adds");
        draws.Compile();
        SubjectMesh mesh;
        mesh.Verts.From = positions.data();
        mesh.Positions = positions;
        mesh.PrevVerts.From = previous.data();
        mesh.Emitted.From = emitted.data();
        mesh.Normals.From = normals.data();
        mesh.Uv.From = uv.data();
        if (mapped) { mesh.Tangents.From = tangents.data(); }
        if (second) { mesh.Uv1.From = uv.data(); }
        if (second) { mesh.Colours.From = colours.data(); }
        mesh.VertexCount = 3;
        mesh.Indices = indices.data();
        mesh.IndexCount = 3;
        mesh.Draws = &draws;
        const auto ticket = draw.BeginMesh(mesh);
        CHECK(ticket.has_value(), "main mesh admitted after the independent piece");
        if (!ticket) { continue; }
        CHECK(draw.FinishMesh(*ticket, mesh, error), "main mesh uploads complete");
        CHECK((resident.HeldOf(Stream::Colour) != 0) == second,
              "only a colour consumer reserves the colour stream");
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
        CHECK(verify(Stream::Uv, uv, resident.SubjectVertices().First),
              "main UVs reach their independently addressed GPU range");
        if (mapped) {
          CHECK(verify(Stream::Tangent, tangents, resident.SubjectVertices().First),
                "main tangents reach their independently addressed GPU range");
        }
        if (second) {
          CHECK(verify(Stream::Uv1, uv, resident.SubjectVertices().First),
                "main second UVs reach their independently addressed GPU range");
          CHECK(verify(Stream::Colour, colours, resident.SubjectColours().First),
                "main colours reach their independently addressed GPU range");
        }
        const auto previousBytes = resident.HeldOf(Stream::Previous);
        CHECK(previousBytes != 0 &&
                  verify(Stream::Previous, previous, resident.SubjectVertices().First),
              "subject previous pose remains independently addressed after a rigid piece");
        const auto pieceFirst = resident.VertexRoom();
        const auto pieceColourFirst = resident.SubjectColours().Count;
        CHECK(draw.PlacePiece(PieceMesh{.Tangents = tangents,
                                        .Verts = vertices,
                                        .Indices = indices,
                                        .Colours = colours,
                                        .Textured = true},
                              error) != kNoPiece,
              "independent tangent-bearing piece reserves and uploads its own range");
        CHECK(draw.HandTables(error) && draw.HandPlacements(false, error),
              "independent pieces publish their explicit colour addressing");
        Readback placements;
        const auto colourSlot = draw.Drawn().back().ModelSlot;
        CHECK(placements.FromBuffer(device.Get(),
                                    resident.Buffer(Stream::Placements).Get(),
                                    (colourSlot + 1) * sizeof(GpuPlacement)) == ReadState::Ready,
              "placement addressing reads from the actual GPU buffer");
        if (placements.Rows() != nullptr) {
          GpuPlacement placement;
          std::memcpy(
              &placement, placements.Rows() + colourSlot * sizeof(GpuPlacement), sizeof(placement));
          CHECK(placement.ColourOffset + pieceFirst == pieceColourFirst,
                "GPU placement maps piece indices into compact colour storage");
        }
        CHECK(verify(Stream::Uv, uv, resident.SubjectVertices().First),
              "later piece growth preserves the earlier main UV bytes");
        if (second) {
          CHECK(verify(Stream::Colour, colours, resident.SubjectColours().First),
                "later piece growth preserves the earlier main colour bytes");
        }
        const auto colourBytes = resident.HeldOf(Stream::Colour);
        const auto uvBytes = resident.HeldOf(Stream::Uv);
        for (int extra = 0; extra < 3; ++extra) {
          CHECK(draw.PlacePiece(PieceMesh{.Verts = vertices, .Indices = indices}, error) !=
                    kNoPiece,
                "untextured untinted pieces extend the arena beyond its spare capacity");
        }
        CHECK(resident.HeldOf(Stream::Colour) == colourBytes &&
                  resident.HeldOf(Stream::Uv) == uvBytes,
              "arena growth without consumers does not grow optional streams");
        CHECK(resident.HeldOf(Stream::Previous) == previousBytes &&
                  verify(Stream::Previous, previous, resident.SubjectVertices().First),
              "rigid arena growth does not expand deformable pose storage");
        const SubjectMesh empty;
        const auto cleared = draw.BeginMesh(empty);
        CHECK(cleared.has_value(), "main mesh clears without retiring independent pieces");
        CHECK(draw.PiecesStanding() == 5 && verify(Stream::Tangent, tangents, pieceFirst) &&
                  verify(Stream::Colour, colours, pieceColourFirst),
              "independent tangent and colour data survive removal of the main mesh");
      }
    }
  }
  SDL_Quit();
  return Report();
}
