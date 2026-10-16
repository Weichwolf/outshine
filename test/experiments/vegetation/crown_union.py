"""Bounded cell unions of cached crown wood; retain canonical source ownership."""

import time

import manifold3d as manifold
import numpy as np
from collar_union import transfer_normals
from outshine_ref.vegetation.tube_topology import close_rings


def overlapped_shoot(mesh):
    rings = mesh["vertices"][:-2].reshape(4, 4, 3)
    normal = np.empty_like(mesh["vertices"])
    normal[mesh["faces"][:-8]] = mesh["normals"].reshape(-1, 3, 3)[:-8]
    normal = normal[:-2].reshape(4, 4, 3)
    tangent = -mesh["normals"][-24]
    depth = float(np.linalg.norm(rings[0, 0] - mesh["vertices"][-2]))
    surface = np.concatenate(((rings[0] - depth * tangent)[None], rings))
    normals = np.concatenate((normal[:1], normal))
    centres = np.concatenate(
        ((mesh["vertices"][-2] - depth * tangent)[None], rings.mean(axis=1))
    )
    centres[-1] = mesh["vertices"][-1]
    tangents = np.broadcast_to(tangent, centres.shape).copy()
    tangents[-1] = mesh["normals"][-1]
    result = close_rings(surface, normals, centres, tangents)
    # Existing side triangles are retained exactly after the prepended ring.
    np.testing.assert_array_equal(
        result["vertices"][result["faces"][8:-8]], mesh["vertices"][mesh["faces"][:-8]]
    )
    return result | {
        "source_kind": 1,
        "source_node": mesh["source_node"],
        "cell": -1,
        "forbidden_caps": tuple(range(32, 36)),
        "root_overlap_m": depth,
    }


def ring_cells(mesh, stations, owner):
    rings = mesh["vertices"][:-2].reshape(stations, 8, 3)
    centre = rings.mean(axis=1)
    centre[0], centre[-1] = mesh["vertices"][-2:]
    tangent = -np.cross(rings[:, 1] - rings[:, 0], rings[:, 2] - rings[:, 0])
    tangent /= np.linalg.norm(tangent, axis=1)[:, None]
    normal = np.empty_like(mesh["vertices"])
    normal[mesh["faces"][:-16]] = mesh["normals"].reshape(-1, 3, 3)[:-16]
    normal = normal[:-2].reshape(stations, 8, 3)
    for index in range(stations - 1):
        piece = close_rings(
            rings[index : index + 2],
            normal[index : index + 2],
            centre[index : index + 2],
            tangent[index : index + 2],
        )
        # Side faces are unchanged; only shared interior caps are temporary.
        np.testing.assert_array_equal(
            piece["vertices"][:16], rings[index : index + 2].reshape(-1, 3)
        )
        piece.update(
            source_kind=0,
            source_node=owner,
            cell=index,
            forbidden_caps=tuple(range(16, 24)) if index else (),
        )
        if index + 1 < stations - 1:
            piece["forbidden_caps"] += tuple(range(24, 32))
        yield piece


def wood_sources(body, paths, along, wood, hosts, representation="cells"):
    if (
        representation not in ("cells", "limbs")
        or type(along) is not int
        or not 1 <= along <= 16
    ):
        raise ValueError("explicit cached limb representation and resolution required")
    stations = [(len(path) - 1) * along + 1 for path in paths]
    count = sum(n - 1 for n in stations) if representation == "cells" else len(paths)
    count += len(hosts)
    faces = (
        sum(n - 1 for n in stations) * 32
        if representation == "cells"
        else len(body["faces"])
    ) + len(hosts) * 40
    if count > 8192 or faces > 262144:
        return None
    products, fo, vo = [], 0, 0
    for path, n in zip(paths, stations, strict=True):
        nv, nf = n * 8 + 2, n * 16
        mesh = {
            "vertices": body["vertices"][vo : vo + nv],
            "faces": body["faces"][fo : fo + nf] - vo,
            "normals": body["normals"][fo * 3 : (fo + nf) * 3],
        }
        if representation == "cells":
            products.extend(ring_cells(mesh, n, path[1]))
        else:
            mesh.update(source_kind=0, source_node=path[1], cell=-1, forbidden_caps=())
            products.append(mesh)
        fo += nf
        vo += nv
    assert fo == len(body["faces"]) and vo == len(body["vertices"])
    for name in ("vertices", "faces", "normals"):
        np.testing.assert_array_equal(wood[name][: len(body[name])], body[name])
    assert len(wood["faces"]) == fo + len(hosts) * 32
    assert len(wood["vertices"]) == vo + len(hosts) * 18
    for index, host in enumerate(hosts):
        v, f = vo + index * 18, fo + index * 32
        products.append(
            overlapped_shoot(
                {
                    "vertices": wood["vertices"][v : v + 18],
                    "faces": wood["faces"][f : f + 32] - v,
                    "normals": wood["normals"][f * 3 : (f + 32) * 3],
                    "source_kind": 1,
                    "source_node": int(host),
                    "cell": -1,
                    "forbidden_caps": tuple(range(24, 28)),
                }
            )
        )
    assert len(products) == count and sum(len(p["faces"]) for p in products) == faces
    return products


def flat_fragment(raw):
    vertices = np.asarray(raw.vert_properties)[:, :3]
    if len(raw.tri_verts) > 8 or len(vertices) > 6:
        return None
    relative = vertices - vertices.mean(axis=0)
    _, _, frame = np.linalg.svd(relative, full_matrices=False)
    thickness = float(np.max(abs(relative @ frame[-1])))
    resolution = float(128 * np.finfo(float).eps * max(1.0, np.max(abs(vertices))))
    if thickness > resolution:
        return None
    return {
        "vertices_m": vertices.tolist(),
        "faces": np.asarray(raw.tri_verts).tolist(),
        "maximum_plane_distance_m": thickness,
        "resolution_m": resolution,
    }


def join_sources(products, max_output_faces=262144):
    if (
        not products
        or len(products) > 8192
        or sum(len(p["faces"]) for p in products) > 262144
    ):
        raise ValueError("bounded complete cached source request required")
    if type(max_output_faces) is not int or not 0 <= max_output_faces <= 262144:
        raise ValueError("bounded explicit output capacity required")
    if max_output_faces == 0:
        return None
    began = time.perf_counter()
    solids, sources = [], {}
    copied = 0
    for mesh in products:
        vertices = np.array(mesh["vertices"], dtype=np.float64, order="C", copy=True)
        faces = np.array(mesh["faces"], dtype=np.uint64, order="C", copy=True)
        ids = np.arange(len(faces), dtype=np.uint64)
        copied += vertices.nbytes + faces.nbytes
        raw = manifold.Mesh64(vertices, faces, face_id=ids)
        solid = manifold.Manifold(raw)
        if solid.status() != manifold.Error.NoError or solid.volume() <= 0:
            raise ValueError("invalid or nonpositive cached source solid")
        keys = solid.to_mesh64().run_original_id
        if len(keys) != 1:
            raise ValueError("one origin per source component required")
        sources[keys[0]] = mesh
        solids.append(solid)
    prepared = time.perf_counter()
    result = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)
    if result.status() != manifold.Error.NoError or result.is_empty():
        raise ValueError("complete cached wood union failed")
    parts, removed = [], []
    for part in result.decompose():
        fragment = flat_fragment(part.to_mesh64())
        if fragment is None:
            parts.append(part)
        else:
            removed.append(fragment)
    if len(parts) != 1 or parts[0].volume() <= 0:
        raise ValueError("whole wood does not form one positive connected body")
    raw = parts[0].to_mesh64()
    if len(raw.tri_verts) > max_output_faces:
        return None
    if len(raw.merge_from_vert):
        raise ValueError("unexpected split positional vertices")
    united = time.perf_counter()
    mesh = transfer_normals(raw, sources)
    kind = np.empty(len(mesh["faces"]), np.uint8)
    node = np.empty(len(mesh["faces"]), np.uint32)
    face_ids = np.asarray(raw.face_id)
    for a, b, key in zip(
        np.asarray(raw.run_index)[:-1] // 3,
        np.asarray(raw.run_index)[1:] // 3,
        raw.run_original_id,
        strict=True,
    ):
        source = sources[key]
        forbidden = np.isin(face_ids[a:b], source["forbidden_caps"])
        if forbidden.any():
            raise ValueError(
                f"internal cap survived: kind={source['source_kind']} node={source['source_node']} cell={source['cell']} ids={face_ids[a:b][forbidden].tolist()}"
            )
        kind[a:b], node[a:b] = source["source_kind"], source["source_node"]
    p = mesh["vertices"][mesh["faces"]]
    cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    if not np.all(
        np.sum(cross[:, None] * mesh["normals"].reshape(-1, 3, 3), axis=2) > 0
    ):
        raise ValueError("union facets disagree with source corner normals")
    mesh.update(source_kind=kind, source_node=node)
    return mesh, {
        "sources": len(products),
        "source_faces": sum(len(p["faces"]) for p in products),
        "kernel_input_geometry_copy_bytes": copied,
        "prepare_s": prepared - began,
        "union_and_parts_s": united - prepared,
        "transfer_s": time.perf_counter() - united,
        "discarded_flat_fragments": removed,
        "volume_m3": parts[0].volume(),
        "genus": parts[0].genus(),
        "output_faces": len(mesh["faces"]),
        "output_array_bytes": sum(v.nbytes for v in mesh.values()),
    }
