"""Inspect frozen axes/windows, then assemble only the requested representation."""

import hashlib
import json

import numpy as np
from conifer_whole_products import load_catalog
from outshine_ref.vegetation import Profile
from outshine_ref.vegetation.axis_surface import axis_mesh


def arrays(path):
    with np.load(path, allow_pickle=False) as data:
        return {k: data[k] for k in data.files}


def source(whole, execution, catalog):
    manifest = json.loads((whole / "plant-product.json").read_text())
    if manifest["version"] not in ("whole-plant-views-1", "cached-window-plant-1"):
        raise ValueError("supported complete cached plant representation required")
    recipe = json.loads((whole / manifest["recipe"]).read_text())
    report = json.loads((execution / "execution.json").read_text())
    if (
        report["source_modified"]
        or manifest["plan_identity"]
        != hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    ):
        raise ValueError("unchanged executed source and matching whole recipe required")
    if not 0 < report["axes"] <= 60000 or not 0 < report["windows"] <= 80000:
        raise ValueError("bounded complete axis/window demand required before decode")
    paths = [
        whole / "plant-product.json",
        whole / manifest["recipe"],
        whole / "windows.npz",
    ]
    paths += [execution / n for n in ("axes.npz", "graph.json", "execution.json")]
    paths += [
        catalog / "source.json",
        catalog / "catalog.json",
        *sorted(catalog.glob("family-*.npz")),
    ]
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    for p in paths[3:]:
        matching = [
            h
            for key, h in manifest["input_sha256"].items()
            if key.endswith("/" + p.name)
        ]
        if matching != [hashes[str(p)]]:
            raise ValueError(
                "executed geometry/catalog must match whole-product provenance"
            )
    graph = json.loads((execution / "graph.json").read_text())
    if graph != recipe["wood"]:
        raise ValueError("same complete executed growth graph required")
    catalog_recipe = json.loads((catalog / "source.json").read_text())["recipe"]
    if catalog_recipe["needles"] != recipe["physical_source"]["needles"]:
        raise ValueError("same physical needle recipe required")
    axes, windows = arrays(execution / "axes.npz"), arrays(whole / "windows.npz")
    if (
        len(axes["origins"]) != report["axes"]
        or len(windows["origins"]) != report["windows"]
    ):
        raise ValueError("complete frozen axis/window counts required")
    first, count = windows["first_needle_ids"], windows["needle_counts"]
    if int(first[0]) != 0 or np.any(count == 0):
        raise ValueError("complete nonempty physical needle range required")
    np.testing.assert_array_equal(first[1:], first[:-1] + count[:-1])
    expected_needles = sum(report["needle_counts"])
    if (
        int(first[-1]) + int(count[-1]) != expected_needles
        or int(axes["needle_counts"].sum()) != expected_needles
    ):
        raise ValueError("entire source needle ledger required")
    owners = windows["axis_ids"]
    if owners.dtype.kind not in "iu" or np.any(
        (owners < 0) | (owners >= len(axes["origins"]))
    ):
        raise ValueError("existing executed axis owners required")
    np.testing.assert_array_equal(
        np.bincount(
            owners.astype(np.int64), weights=count, minlength=len(axes["origins"])
        ),
        axes["needle_counts"],
    )
    metadata, banks = load_catalog(catalog)
    for info in metadata.values():
        mask = windows["families"] == info["family"]
        np.testing.assert_allclose(
            windows["weights"][mask], count[mask] / info["needles"], rtol=0, atol=1e-15
        )
    for key in ("frames", "origins", "weights"):
        if not np.isfinite(windows[key]).all():
            raise ValueError("finite physical window attributes required")
    if np.any(
        (windows["families"] < 0) | (windows["families"] >= len(banks))
    ) or np.any((windows["weights"] <= 0) | (windows["weights"] > 1)):
        raise ValueError("bounded physical families and tail fractions required")
    np.testing.assert_allclose(
        windows["frames"] @ windows["frames"].transpose(0, 2, 1),
        np.broadcast_to(np.eye(3), windows["frames"].shape),
        rtol=0,
        atol=2e-12,
    )
    if np.any(np.linalg.det(windows["frames"]) <= 0):
        raise ValueError("proper source frames required, not reflections")
    return (
        graph,
        axes,
        windows,
        banks,
        {
            "identity": manifest["plan_identity"],
            "files": hashes,
            "file_bytes": sum(p.stat().st_size for p in paths),
        },
    )


def wood_templates(graph):
    templates = []
    for order in range(4):
        shape = graph["nodes"][order + 1]["shape"]
        radius = Profile(np.array(shape["radius"]["table"]), shape["radius"]["kind"])
        templates.append(
            axis_mesh(
                shape["length"],
                shape["bend"],
                float(radius.evaluate(0)),
                float(radius.evaluate(1)),
                4 if order == 0 else 2,
                8,
            )
        )
    return templates


def wood_orders(axes):
    orders = np.diff(axes["path_offsets"]).astype(np.int64) - 2
    if np.any((orders < 0) | (orders > 3)) or not 0 < len(orders) <= 60000:
        raise ValueError("bounded four-order executed wood required")
    return orders


def wood(graph, axes):
    orders = wood_orders(axes)
    templates = wood_templates(graph)
    counts = np.bincount(orders, minlength=4)
    face_count = sum(int(n) * len(m["faces"]) for n, m in zip(counts, templates))
    if not 0 < face_count <= 3000000:
        raise ValueError("wood triangle budget required before expansion")
    vertex_count = sum(int(n) * len(m["vertices"]) for n, m in zip(counts, templates))
    result = {
        "vertices": np.empty((vertex_count, 3), dtype=np.float32),
        "faces": np.empty((face_count, 3), dtype=np.int32),
        "normals": np.empty((face_count * 3, 3), dtype=np.float32),
    }
    vertex, face = 0, 0
    for order, mesh in enumerate(templates):
        indices = np.flatnonzero(orders == order)
        for begin in range(0, len(indices), 1024):
            selected = indices[begin : begin + 1024]
            frames = axes["rotations"][selected]
            points = np.einsum("vj,nij->nvi", mesh["vertices"], frames)
            points *= axes["uniform_scales"][selected, None, None]
            points += axes["origins"][selected, None]
            nv, nf = (
                len(selected) * len(mesh["vertices"]),
                len(selected) * len(mesh["faces"]),
            )
            result["vertices"][vertex : vertex + nv] = points.reshape(-1, 3)
            result["normals"][face * 3 : (face + nf) * 3] = np.einsum(
                "vj,nij->nvi", mesh["normals"], frames
            ).reshape(-1, 3)
            first = np.arange(len(selected)) * len(mesh["vertices"]) + vertex
            result["faces"][face : face + nf] = (
                mesh["faces"][None] + first[:, None, None]
            ).reshape(-1, 3)
            vertex, face = vertex + nv, face + nf
    return result
