"""Compare strict coverage alternatives without changing other constructor checks."""

import contextlib
import contextvars
from unittest.mock import patch

import shapely
from outshine_ref.geometry.surface_patches import SurfacePatch
from outshine_ref.geometry.surface_patches import _patch as patch_module
from outshine_ref.geometry.surface_patches._coverage import proves_single_boundary
from stair_coverage_boundary import GENERAL_COVERAGE, mesh_witness, polygon_witness


@contextlib.contextmanager
def validation(mode, records):
    if mode not in ("general", "boundary", "mesh", "mesh-batched", "library"):
        raise ValueError(
            "explicit general/polygon-adapter/mesh coverage alternative required"
        )
    if mode == "library":

        def observed(xy, faces, polygons):
            proved = proves_single_boundary(xy, faces, polygons)
            records.append({"proved": proved, "input_polygons": len(polygons)})
            return proved

        with patch.object(patch_module, "proves_single_boundary", observed):
            yield
        return
    current = contextvars.ContextVar("coverage_research_mesh", default=None)
    original_init = SurfacePatch.__post_init__

    def initialise(instance):
        token = current.set(instance.mesh)
        try:
            return original_init(instance)
        finally:
            current.reset(token)

    def coverage(polygons):
        mesh = current.get()
        if mode == "general" or mesh is None:
            proof = {"proved": False, "reason": "general_or_outside_constructor"}
        elif mode in ("mesh", "mesh-batched"):
            proof = mesh_witness(
                mesh, polygons, ring_batch=1024 if mode == "mesh-batched" else None
            )
        else:
            proof = polygon_witness(polygons)
        valid = proof["proved"] or bool(GENERAL_COVERAGE(polygons))
        records.append(dict(proof, valid=valid, input_polygons=len(polygons)))
        return valid

    with (
        patch.object(SurfacePatch, "__post_init__", initialise),
        patch.object(patch_module, "proves_single_boundary", lambda *_a: False),
        patch.object(shapely, "coverage_is_valid", coverage),
    ):
        yield
