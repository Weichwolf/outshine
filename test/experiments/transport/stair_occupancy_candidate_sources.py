"""Bound metadata of verified body facets; selected references use old builders."""

import json
from pathlib import Path

import numpy as np
from stair_module_inputs import reference_profiles
from stair_occupancy_candidates import CandidateVolumes, VolumeSource
from stair_occupancy_inputs import volume
from stair_shared_headroom import bindings
from stair_shared_headroom_volumes import supplement


def numeric_bounds(lower, upper, clearance):
    lo = np.minimum(lower.min(1), upper.min(1))
    hi = np.maximum(lower.max(1), upper.max(1))
    hi[:, 2] += clearance
    # The existing affine builder enforces <=1e-8 m outward error at each cap knot.
    lo[:, 2] = np.nextafter(lo[:, 2] - 1e-8, -np.inf)
    hi[:, 2] = np.nextafter(hi[:, 2] + 1e-8, np.inf)
    result = np.c_[lo, hi]
    result.setflags(write=False)
    return result


def road_keys(asset, face):
    return tuple(
        k
        for k, used in zip(asset.source_ids, asset.data["owners"][face], strict=True)
        if used
    )


def road_clearance(asset, sources):
    profiles = reference_profiles()
    result = np.zeros(len(asset.data["top_faces"]))
    if not asset.data["owners"].any(axis=1).all():
        raise ValueError("every physical road facet needs its actual source owner")
    for column, key in enumerate(asset.source_ids):
        kind = sources[key]["properties"].get("subclass") or "unclassified"
        if kind not in profiles:
            raise ValueError("explicit source headroom recipe missing")
        np.maximum(
            result,
            profiles[kind]["clearanceM"],
            out=result,
            where=asset.data["owners"][:, column],
        )
    return result


def road_source(asset, sources):
    data, clearance = asset.data, road_clearance(asset, sources)
    bounds = numeric_bounds(
        data["body_vertices"][data["bottom_faces"]],
        data["body_vertices"][data["top_faces"]],
        clearance,
    )

    def make(face):
        row, error = volume(
            f"{asset.identity}:face:{face}",
            data["body_vertices"][data["bottom_faces"][face]],
            data["body_vertices"][data["top_faces"][face]],
            float(clearance[face]),
        )
        return ((row, road_keys(asset, face), error),)

    return VolumeSource(asset.identity + ":road", bounds, make)


def support_sources(supports, contacts, manifest):
    report = json.loads(
        (Path(manifest["support_mesh"]).parent / "comparison.json").read_text()
    )
    stations = report["methods"][report["selected"]]["stations"]
    owners = {r["product"]: r["source_id"] for r in stations if "product" in r}
    if set(owners) != set(range(len(supports))):
        raise ValueError("whole original support ownership required")
    result = []
    for index, (body, (low, high)) in enumerate(zip(supports, contacts, strict=True)):
        if not (
            np.array_equal(body.vertices, np.vstack((high.vertices, low.vertices)))
            and np.array_equal(body.triangles[: len(high.triangles)], high.triangles)
            and np.array_equal(
                body.triangles[len(high.triangles) : 2 * len(high.triangles)],
                low.triangles[:, ::-1] + len(high.vertices),
            )
        ):
            raise ValueError("whole original support caps required")

        def make(face, index=index, low=low, high=high):
            row, error = volume(
                f"{manifest['support_sha256']}:support:{index}:face:{face}",
                low.vertices[high.triangles[face]],
                high.vertices[high.triangles[face]],
                0,
            )
            return ((row, (owners[index],), error),)

        bounds = numeric_bounds(
            low.vertices[high.triangles], high.vertices[high.triangles], 0
        )
        result.append(
            VolumeSource(f"{manifest['support_sha256']}:support:{index}", bounds, make)
        )
    return result


def logical_source(chosen, sources):
    old, indices = chosen["old_asset"], chosen["indices"]
    data, clearance = old.data, road_clearance(old, sources)
    bounds = numeric_bounds(
        data["body_vertices"][data["bottom_faces"][indices]],
        data["body_vertices"][data["top_faces"][indices]],
        chosen["required_m"],
    )

    def make(index):
        face = int(indices[index])
        base, error = volume(
            f"{old.identity}:face:{face}",
            data["body_vertices"][data["bottom_faces"][face]],
            data["body_vertices"][data["top_faces"][face]],
            float(clearance[face]),
        )
        rows, owners, _records, _work = supplement(
            (base,),
            (road_keys(old, face),),
            ({**chosen, "indices": np.array([face])},),
            sources,
        )
        return tuple((row, keys, error) for row, keys in zip(rows, owners, strict=True))

    # Conservative pilot: no XY/Z rejection of unmaterialized logical clips.
    return VolumeSource(
        old.identity + ":headroom:" + chosen["source_id"], bounds, make, always=True
    )


def from_state(state, sources, modules=()):
    view, assets, supports, contacts, manifest = state
    chosen = bindings(
        assets,
        () if "borrowed_headroom_contacts" in manifest else modules,
        prepared_contacts=manifest.get("borrowed_headroom_contacts", ()),
    )
    result = [road_source(a, sources) for a in assets]
    result.extend(support_sources(supports, contacts, manifest))
    result.extend(logical_source(c, sources) for c in chosen)
    return CandidateVolumes(view.patches[0].frame, tuple(result))
