"""System GEOS constrained triangulation, matching the engine's polygon backend."""

import ctypes
import ctypes.util
from pathlib import Path

import shapely


def triangles_of(polygon):
    library = ctypes.util.find_library('geos_c')
    if not library:
        installed = Path('/opt/homebrew/opt/geos/lib/libgeos_c.dylib')
        if not installed.is_file():
            raise ValueError('system libgeos_c required')
        library = str(installed)
    lib = ctypes.CDLL(library)
    ptr, size = ctypes.c_void_p, ctypes.c_size_t
    signatures = {
        'GEOS_init_r': ([], ptr),
        'GEOS_finish_r': ([ptr], None),
        'GEOSGeomFromWKB_buf_r': ([ptr, ptr, size], ptr),
        'GEOSConstrainedDelaunayTriangulation_r': ([ptr, ptr], ptr),
        'GEOSGeomToWKB_buf_r': ([ptr, ptr, ctypes.POINTER(size)], ptr),
        'GEOSGeom_destroy_r': ([ptr, ptr], None),
        'GEOSFree_r': ([ptr, ptr], None),
    }
    for name, (arguments, result) in signatures.items():
        function = getattr(lib, name)
        function.argtypes, function.restype = arguments, result
    context = lib.GEOS_init_r()
    if not context:
        raise ValueError('GEOS context allocation failed')
    source = made = wire = None
    try:
        data = polygon.wkb
        source = lib.GEOSGeomFromWKB_buf_r(context, data, len(data))
        if not source:
            raise ValueError('GEOS rejected the source footprint')
        made = lib.GEOSConstrainedDelaunayTriangulation_r(context, source)
        if not made:
            raise ValueError('GEOS constrained triangulation failed')
        length = size()
        wire = lib.GEOSGeomToWKB_buf_r(context, made, ctypes.byref(length))
        if not wire:
            raise ValueError('GEOS triangle export failed')
        return shapely.get_parts(shapely.from_wkb(ctypes.string_at(wire, length.value)))
    finally:
        if wire:
            lib.GEOSFree_r(context, wire)
        for geometry in (made, source):
            if geometry:
                lib.GEOSGeom_destroy_r(context, geometry)
        lib.GEOS_finish_r(context)
