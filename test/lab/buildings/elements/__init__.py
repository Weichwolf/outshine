"""THE GEOMETRY ELEMENTS, one module per family.

Importing this imports every family, which is what registers them. A new family is a new module
and one line here -- there is no dispatcher, and `base.build_all` walks the registry.
"""
from .base import BUILT, ORDER, Place, build_all, catalogue, register  # noqa: F401
from . import relief    # noqa: F401,E402
from . import roofline  # noqa: F401,E402
from . import openings  # noqa: F401,E402
from . import facade    # noqa: F401,E402
from .facade import holed_wall, openings_of  # noqa: F401,E402
