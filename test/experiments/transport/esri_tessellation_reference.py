"""Public GLU-style C ABI of the SDK's external tessellation dependency only."""

import ctypes as c
import hashlib
from pathlib import Path
import time

import numpy as np
import shapely

from esri_transport_catalog import CORE


DEPENDENCY_SHA256 = "2b10c97d5a3c92a2885786d4e0d497cb753b9786ef0b450db1ee61381c1fda78"
BEGIN, VERTEX, END, ERROR, COMBINE = 100100, 100101, 100102, 100103, 100105


def triangulate(request):
    raw = request.get("rings")
    if not isinstance(raw, list) or not 1 <= len(raw) <= 8 or any(not isinstance(row, list) for row in raw) or sum(map(len, raw)) > 512:
        raise ValueError("ring/vertex caps exhausted before numeric conversion")
    rings = [np.asarray(row, dtype=np.float64) for row in request["rings"]]
    if not 1 <= len(rings) <= 8 or any(row.ndim != 2 or row.shape[1] != 2 or len(row) < 3 or not np.isfinite(row).all() for row in rings):
        raise ValueError("finite exterior/hole rings required")
    if sum(map(len, rings)) > 512:
        raise ValueError("tessellation input vertex cap exceeded")
    polygon = shapely.Polygon(rings[0], rings[1:])
    if polygon.is_empty or not polygon.is_valid:
        raise ValueError("reference only accepts a valid explicit polygon domain")
    path = CORE.parent / "libglutess.so"
    if hashlib.sha256(path.read_bytes()).hexdigest() != DEPENDENCY_SHA256:
        raise ValueError("SDK tessellation dependency differs from the inspected binary")
    library = c.CDLL(str(path))
    prototypes = dict(NewTess=([], c.c_void_p), DeleteTess=([c.c_void_p], None),
                      TessBeginPolygon=([c.c_void_p, c.c_void_p], None), TessEndPolygon=([c.c_void_p], None),
                      TessBeginContour=([c.c_void_p], None), TessEndContour=([c.c_void_p], None),
                      TessCallback=([c.c_void_p, c.c_uint, c.c_void_p], None),
                      TessNormal=([c.c_void_p, c.c_double, c.c_double, c.c_double], None),
                      TessProperty=([c.c_void_p, c.c_uint, c.c_double], None),
                      TessVertex=([c.c_void_p, c.c_void_p, c.c_void_p], None))
    functions = {}
    for name, (args, result) in prototypes.items():
        function = getattr(library, "glutess" + name)
        function.argtypes, function.restype = args, result
        functions[name] = function
    points, faces, stream, errors, callback_buffers = [], [], [], [], []
    primitive = [0]

    def fail(message):
        if not errors:
            errors.append(message)

    def begin(mode):
        primitive[0] = mode
        stream.clear()

    def vertex(data):
        if data is None or not 1 <= data <= len(points) or len(stream) >= 6144:
            fail("vertex callback exceeds bounded ID/stream contract")
            return
        stream.append(data - 1)

    def end():
        mode = primitive[0]
        if mode == 4 and len(stream) % 3 == 0:
            rows = [stream[i:i + 3] for i in range(0, len(stream), 3)]
        elif mode == 5:
            rows = [([stream[i], stream[i + 1], stream[i + 2]] if i % 2 == 0 else
                     [stream[i + 1], stream[i], stream[i + 2]]) for i in range(len(stream) - 2)]
        elif mode == 6:
            rows = [[stream[0], stream[i], stream[i + 1]] for i in range(1, len(stream) - 1)]
        else:
            fail("unexpected SDK primitive")
            return
        if len(faces) + len(rows) > 2048:
            fail("SDK output face cap exceeded")
        else:
            faces.extend(rows)

    def error(code):
        fail("SDK tessellation error " + str(code))

    def combine(coordinates, data, weights, output):
        if len(points) >= 1024:
            fail("SDK combined vertex cap exceeded")
            output[0] = None
            return
        points.append([coordinates[i] for i in range(3)])
        output[0] = len(points)

    callbacks = [c.CFUNCTYPE(None, c.c_uint)(begin), c.CFUNCTYPE(None, c.c_void_p)(vertex), c.CFUNCTYPE(None)(end),
                 c.CFUNCTYPE(None, c.c_uint)(error),
                 c.CFUNCTYPE(None, c.POINTER(c.c_double), c.POINTER(c.c_void_p), c.POINTER(c.c_float), c.POINTER(c.c_void_p))(combine)]
    began = time.perf_counter_ns()
    tessellator = functions["NewTess"]()
    if not tessellator:
        raise ValueError("SDK tessellator allocation failed")
    try:
        for which, callback in zip((BEGIN, VERTEX, END, ERROR, COMBINE), callbacks):
            functions["TessCallback"](tessellator, which, c.cast(callback, c.c_void_p))
        functions["TessNormal"](tessellator, 0., 0., 1.)
        functions["TessProperty"](tessellator, 100140, 100130.)
        functions["TessBeginPolygon"](tessellator, None)
        for ring in rings:
            functions["TessBeginContour"](tessellator)
            for xy in ring:
                position = (c.c_double * 3)(xy[0], xy[1], 0.)
                callback_buffers.append(position)
                points.append([xy[0], xy[1], 0.])
                functions["TessVertex"](tessellator, position, len(points))
            functions["TessEndContour"](tessellator)
        functions["TessEndPolygon"](tessellator)
    finally:
        functions["DeleteTess"](tessellator)
    elapsed = time.perf_counter_ns() - began
    if errors or not faces:
        raise ValueError("; ".join(errors) or "SDK returned no triangles")
    return dict(vertices=points, triangles=faces, operation_and_callbacks_ns=elapsed,
                dependency_path=str(path), dependency_sha256=DEPENDENCY_SHA256, c_abi="documented GLU tessellation API with SDK glutess prefix",
                input_vertices=sum(map(len, rings)), combined_vertices=len(points) - sum(map(len, rings)),
                scope="SDK-linked tessellation backend only; util::Triangulator modes, material/corner interpolation and ownership are not executable-verified")
