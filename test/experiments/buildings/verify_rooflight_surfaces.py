"""Independent physical/contact and ray controls of direct rooflight skins."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
from facade_parts import append
from rooflight_surfaces import closed, contact, corners, prepare_host, template
from structural_shell_geometry import native
from trimesh.ray.ray_triangle import ray_triangle_id
from verify_industrial_halls import sources
from verify_industrial_rooflights import generate


class AllTriangles:
    """Supply every triangle to Trimesh's independent narrow phase, without an index."""

    def __init__(self, triangles):
        points = triangles.reshape(-1, 3)
        self.bounds = np.r_[points.min(axis=0), points.max(axis=0)]
        self.count = len(triangles)

    def intersection(self, bounds):
        return range(self.count)


def first_hits(mesh, origins, directions):
    triangles = mesh["vertices"][mesh["faces"]]
    tree = AllTriangles(triangles)
    distance = np.full(len(origins), np.inf)
    materials = np.full(len(origins), -1)
    normals = np.zeros((len(origins), 3))
    for start in range(0, len(origins), 32):
        stop = min(start + 32, len(origins))
        face, ray, positions = ray_triangle_id(
            triangles,
            origins[start:stop],
            directions[start:stop],
            tree=tree,
            multiple_hits=False,
        )
        distance[start + ray] = np.linalg.norm(positions - origins[start + ray], axis=1)
        materials[start + ray] = mesh["materials"][face]
        normals[start + ray] = mesh["normals"][mesh["faces"][face, 0]]
    return distance, materials, normals


def probes(item, hall):
    normal = np.r_[-item["slope"] * np.asarray(item["axes"])[0], 1.0]
    normal /= np.linalg.norm(normal)
    rng = np.random.default_rng(417)
    origins, targets = [], []
    xy = corners(item)
    for _ in range(96):
        u, v = rng.uniform(0.03, 0.97, 2)
        target = xy[0] + u * (xy[1] - xy[0]) + v * (xy[3] - xy[0])
        # Air above the owned roof plane; lateral displacement remains in its plane.
        eye = (
            target
            + normal * 2
            + np.r_[np.asarray(item["axes"])[1] * rng.uniform(-2, 2), 0]
        )
        origins.append(eye)
        targets.append(target - normal * 0.25)
    for _ in range(64):
        u, v = rng.uniform(0.2, 0.8, 2)
        target = xy[0] + u * (xy[1] - xy[0]) + v * (xy[3] - xy[0])
        eye = target.copy()
        eye[2] = (target[2] + hall["ceiling_m"]) / 2 - 0.08
        origins.append(eye)
        targets.append(target + normal * 0.5)
    origins, targets = np.asarray(origins), np.asarray(targets)
    directions = targets - origins
    directions /= np.linalg.norm(directions, axis=1, keepdims=True)
    return origins, directions


class RooflightSurfaces(unittest.TestCase):
    def test_direct_closed_components_have_the_original_actual_solid(self):
        for row, neighbors in sources()[:3]:
            _mesh, _shell, _body, original, fields, *_ = generate(row, neighbors)
            direct = closed(fields)
            old = manifold.Manifold.batch_boolean(
                list(map(native, original)), manifold.OpType.Add
            )
            new = manifold.Manifold.batch_boolean(
                list(map(native, direct)), manifold.OpType.Add
            )
            self.assertEqual(new.status(), manifold.Error.NoError)
            self.assertLess((old - new).volume() + (new - old).volume(), 1e-7)
            self.assertEqual(sum(len(p["faces"]) for p in direct), len(fields) * 44)

    def test_opaque_mating_hosts_keep_first_hits_and_closed_glass_from_both_sides(self):
        for row, neighbors in sources()[:3]:
            _mesh, shell, body, original, fields, hall, *_ = generate(row, neighbors)
            host = prepare_host(shell, fields)
            skin = contact(host)
            self.assertEqual(sum(len(p["faces"]) for p in skin), len(fields) * 36)
            full = append(body, original)
            compact = append(body, skin)
            for item in (fields[0], fields[len(fields) // 2], fields[-1]):
                origins, directions = probes(item, hall)
                first = first_hits(full, origins, directions)
                second = first_hits(compact, origins, directions)
                np.testing.assert_allclose(first[0], second[0], atol=1e-7, rtol=0)
                np.testing.assert_array_equal(first[1], second[1])
                np.testing.assert_allclose(first[2], second[2], atol=1e-7, rtol=0)

    def test_missing_shifted_incomplete_and_wrong_material_hosts_reject(self):
        row, neighbors = sources()[1]
        _mesh, shell, _body, _original, fields, *_ = generate(row, neighbors)
        with self.assertRaisesRegex(ValueError, "owned lightwell"):
            prepare_host(
                replace(
                    shell, source_ids=tuple("other-" + s for s in shell.source_ids)
                ),
                fields,
            )
        raw = dict(shell.mesh)
        xyz = raw["vertices"].copy()
        xyz[:, 0] += 0.01
        xyz.flags.writeable = False
        with self.assertRaisesRegex(ValueError, "mating wall|uncovered frame"):
            prepare_host(replace(shell, mesh=dict(raw, vertices=xyz)), fields)
        mats = raw["materials"].copy()
        mats[
            shell.source_indices
            == shell.source_ids.index(fields[0]["id"] + ":lightwell")
        ] = 3
        mats.flags.writeable = False
        with self.assertRaisesRegex(ValueError, "unexpected material"):
            prepare_host(replace(shell, mesh=dict(raw, materials=mats)), fields)
        selected = shell.source_indices == shell.source_ids.index(
            fields[0]["id"] + ":lightwell"
        )
        changed = raw["faces"].copy()
        changed[selected] = changed[~selected][0]
        changed.flags.writeable = False
        with self.assertRaisesRegex(ValueError, "mating wall|uncovered frame"):
            prepare_host(replace(shell, mesh=dict(raw, faces=changed)), fields)
        with self.assertRaises(TypeError):
            contact(fields)

    def test_direct_products_never_expand_cga_prisms_or_mutate_the_cached_host(self):
        row, neighbors = sources()[1]
        _mesh, shell, _body, _original, fields, *_ = generate(row, neighbors)
        before = {k: v.copy() for k, v in shell.mesh.items()}
        host = prepare_host(shell, fields)
        template.cache_clear()
        with patch(
            "industrial_rooflights.region_solid",
            side_effect=AssertionError("unneeded CGA extrusion"),
        ):
            self.assertEqual(len(closed(fields)), len(fields))
            self.assertEqual(len(contact(host)), len(fields))
        for key, value in before.items():
            np.testing.assert_array_equal(shell.mesh[key], value)
        with self.assertRaises(TypeError):
            host.fields[0]["id"] = "changed"
        for invalid in (
            (0.01, 4, 1),
            (2, 0.01, 1),
            (2, 4, float("nan")),
            (2, 4, float("inf")),
        ):
            with self.assertRaises(ValueError):
                template(*invalid)
        with self.assertRaisesRegex(ValueError, "capacity"):
            closed([fields[0]] * 65)

    def test_contact_skin_alone_is_not_an_independent_collision_solid(self):
        row, neighbors = sources()[1]
        _mesh, shell, _body, _original, fields, *_ = generate(row, neighbors)
        skin = contact(prepare_host(shell, fields))
        with self.assertRaisesRegex(ValueError, "Actual structural mesh rejected"):
            native(skin[0])
        self.assertTrue(all(len(p["faces"][p["materials"] == 3]) == 12 for p in skin))


if __name__ == "__main__":
    unittest.main()
