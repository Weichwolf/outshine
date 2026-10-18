"""Optional numeric codec; callers own trusted product records and storage policy.

Known-valid generated products can be restored without repeating geometric
construction. This module performs no IO, cache lookup or independent admission.
"""

from ._archive import FORMAT, MAX_BYTES, pack_surface
from ._restore import restore_surface

__all__ = ["FORMAT", "MAX_BYTES", "pack_surface", "restore_surface"]
