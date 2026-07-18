/* Unit tests for osmmesh terrain_ecef.c — the ECEF terrain mesh builder.
 *
 * Target: 100% line coverage of terrain_ecef.c. Verified by construction:
 *   - origin is the tile centre on the ellipsoid at the centre height
 *   - stored float offsets reproduce the exact double ECEF to sub-cm
 *   - normals are unit length and point OUTWARD (dot with the radial > 0),
 *     and ~radial at the centre of a flat tile (this is what pins the winding:
 *     an inward normal would make the dot negative and this bite)
 *   - every ARG guard rejects.
 * No GL, no screenshots: the geometry is checked against the WGS84 maths.
 */
#include <math.h>
#include <stdlib.h>
#include "osmmesh/geo.h"
#include "osmmesh/terrain.h"
#include "tassert.h"

static void mesh_free_local(osmmesh_mesh *m) {
    free(m->positions); free(m->normals); free(m->uvs); free(m->indices);
    m->positions = NULL; m->normals = NULL; m->uvs = NULL; m->indices = NULL;
}

static double vlen(double x, double y, double z) { return sqrt(x*x + y*y + z*z); }

void test_terrain_ecef(void) {
    printf("== terrain_ecef unit tests ==\n");

    const uint8_t Z = 14; const uint32_t X = 8555, Y = 5424;   /* a z14 tile */
    const uint32_t N = 5;                                       /* 5x5 grid */

    /* varied heightfield: gentle slope, row 0 = north, col 0 = west */
    float hv[25];
    for (uint32_t r = 0; r < N; r++)
        for (uint32_t c = 0; c < N; c++)
            hv[r*N + c] = 100.0f + (float)r * 2.0f + (float)c * 3.0f;
    osmmesh_terrain_grid grid = { hv, N, N };

    osmmesh_terrain_build_opts opts = { 1, 1, 0, 0.0f };   /* stride 1, normals on */
    osmmesh_mesh mesh; double origin[3];

    /* ---- success, normals on ---- */
    int rc = osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, &opts, &mesh, origin);
    ck(rc == OSMMESH_TERRAIN_OK, "build ok");
    ck(mesh.n_vertices == N * N, "vertex count = 25");
    ck(mesh.n_triangles == (N - 1) * (N - 1) * 2, "triangle count = 32");
    ck(mesh.positions && mesh.normals && mesh.uvs && mesh.indices, "all buffers present");

    /* origin = tile centre on the ellipsoid at the centre height */
    osmmesh_geo ocg = osmmesh_tile_frac_to_geo(Z, X, Y, 0.5, 0.5);
    ocg.alt = hv[2*N + 2];
    osmmesh_ecef oce = osmmesh_geo_to_ecef(ocg);
    ck_near(origin[0], oce.x, 1e-6, "origin.x = centre ECEF");
    ck_near(origin[1], oce.y, 1e-6, "origin.y = centre ECEF");
    ck_near(origin[2], oce.z, 1e-6, "origin.z = centre ECEF");

    /* stored float offsets reproduce the exact double ECEF offset to sub-cm,
     * and are small (< 2 km) so float precision holds near the origin */
    double max_off = 0.0, max_err = 0.0;
    for (uint32_t r = 0; r < N; r++) {
        for (uint32_t c = 0; c < N; c++) {
            uint32_t vi = r*N + c;
            double fc = (double)c / (double)(N - 1);
            double fr = (double)r / (double)(N - 1);
            osmmesh_geo g = osmmesh_tile_frac_to_geo(Z, X, Y, fc, fr);
            g.alt = hv[r*N + c];
            osmmesh_ecef p = osmmesh_geo_to_ecef(g);
            double ex = p.x - origin[0], ey = p.y - origin[1], ez = p.z - origin[2];
            double dx = (double)mesh.positions[3*vi+0] - ex;
            double dy = (double)mesh.positions[3*vi+1] - ey;
            double dz = (double)mesh.positions[3*vi+2] - ez;
            double err = vlen(dx, dy, dz);
            double off = vlen(ex, ey, ez);
            if (err > max_err) max_err = err;
            if (off > max_off) max_off = off;
        }
    }
    ck(max_err < 0.01, "float offset error < 1 cm");
    ck(max_off < 2000.0, "offsets < 2 km (float-safe)");

    /* normals unit length, and OUTWARD everywhere (dot with radial > 0).
     * radial = direction from earth centre to the vertex's absolute ECEF. */
    int all_unit = 1, all_outward = 1;
    for (uint32_t v = 0; v < mesh.n_vertices; v++) {
        const float *nrm = &mesh.normals[3*v];
        double len = vlen(nrm[0], nrm[1], nrm[2]);
        if (fabs(len - 1.0) > 1e-4) all_unit = 0;
        double ax = origin[0] + mesh.positions[3*v+0];
        double ay = origin[1] + mesh.positions[3*v+1];
        double az = origin[2] + mesh.positions[3*v+2];
        double rm = vlen(ax, ay, az);
        double dot = (nrm[0]*ax + nrm[1]*ay + nrm[2]*az) / rm;
        if (dot <= 0.0) all_outward = 0;
    }
    ck(all_unit, "normals unit length");
    ck(all_outward, "normals point outward (winding correct)");

    /* UV convention matches the ENU path: (fc, 1-fr) */
    ck_near(mesh.uvs[0], 0.0, 1e-6, "uv[0].u = 0 (west)");
    ck_near(mesh.uvs[1], 1.0, 1e-6, "uv[0].v = 1 (north)");
    uint32_t last = (N*N - 1);
    ck_near(mesh.uvs[2*last+0], 1.0, 1e-6, "uv[last].u = 1 (east)");
    ck_near(mesh.uvs[2*last+1], 0.0, 1e-6, "uv[last].v = 0 (south)");
    mesh_free_local(&mesh);

    /* ---- flat tile: centre normal must be ~radial (winding proof, tight) ---- */
    float hf[25];
    for (int i = 0; i < 25; i++) hf[i] = 200.0f;
    osmmesh_terrain_grid gflat = { hf, N, N };
    rc = osmmesh_terrain_build_mesh_ecef(&gflat, Z, X, Y, &opts, &mesh, origin);
    ck(rc == OSMMESH_TERRAIN_OK, "flat build ok");
    {
        uint32_t vc = 2*N + 2;   /* centre vertex */
        const float *nrm = &mesh.normals[3*vc];
        double ax = origin[0] + mesh.positions[3*vc+0];
        double ay = origin[1] + mesh.positions[3*vc+1];
        double az = origin[2] + mesh.positions[3*vc+2];
        double rm = vlen(ax, ay, az);
        double dot = (nrm[0]*ax + nrm[1]*ay + nrm[2]*az) / rm;
        ck(dot > 0.999, "flat-tile centre normal ~ radial-outward");
    }
    mesh_free_local(&mesh);

    /* ---- success, normals off ---- */
    osmmesh_terrain_build_opts opt_nn = { 1, 0, 0, 0.0f };
    rc = osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, &opt_nn, &mesh, origin);
    ck(rc == OSMMESH_TERRAIN_OK && mesh.normals == NULL, "normals off -> NULL");
    mesh_free_local(&mesh);

    /* ---- stride > 1 that divides (5-1)=4 ---- */
    osmmesh_terrain_build_opts opt_s2 = { 2, 1, 0, 0.0f };
    rc = osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, &opt_s2, &mesh, origin);
    ck(rc == OSMMESH_TERRAIN_OK && mesh.n_vertices == 9, "stride 2 -> 3x3 = 9 verts");
    mesh_free_local(&mesh);

    /* ---- ARG guards ---- */
    ck(osmmesh_terrain_build_mesh_ecef(NULL, Z, X, Y, &opts, &mesh, origin) == OSMMESH_TERRAIN_ERR_ARG, "null grid");
    ck(osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, NULL, &mesh, origin) == OSMMESH_TERRAIN_ERR_ARG, "null opts");
    ck(osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, &opts, NULL, origin) == OSMMESH_TERRAIN_ERR_ARG, "null mesh");
    ck(osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, &opts, &mesh, NULL) == OSMMESH_TERRAIN_ERR_ARG, "null origin_out");

    osmmesh_terrain_grid gnoh = { NULL, N, N };
    ck(osmmesh_terrain_build_mesh_ecef(&gnoh, Z, X, Y, &opts, &mesh, origin) == OSMMESH_TERRAIN_ERR_ARG, "null heights");
    osmmesh_terrain_grid gsmall = { hv, 1, N };
    ck(osmmesh_terrain_build_mesh_ecef(&gsmall, Z, X, Y, &opts, &mesh, origin) == OSMMESH_TERRAIN_ERR_ARG, "rows < 2");
    osmmesh_terrain_grid gsmall2 = { hv, N, 1 };
    ck(osmmesh_terrain_build_mesh_ecef(&gsmall2, Z, X, Y, &opts, &mesh, origin) == OSMMESH_TERRAIN_ERR_ARG, "cols < 2");

    osmmesh_terrain_build_opts opt_s0 = { 0, 1, 0, 0.0f };
    ck(osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, &opt_s0, &mesh, origin) == OSMMESH_TERRAIN_ERR_ARG, "stride 0");
    osmmesh_terrain_build_opts opt_skirt = { 1, 1, 1, 0.0f };
    ck(osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, &opt_skirt, &mesh, origin) == OSMMESH_TERRAIN_ERR_ARG, "add_skirt rejected");
    osmmesh_terrain_build_opts opt_s3 = { 3, 1, 0, 0.0f };   /* 4 % 3 != 0 */
    ck(osmmesh_terrain_build_mesh_ecef(&grid, Z, X, Y, &opt_s3, &mesh, origin) == OSMMESH_TERRAIN_ERR_ARG, "stride not dividing");
}
