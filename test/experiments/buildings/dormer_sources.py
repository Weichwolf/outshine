"""Three real concave/courtyard urban roofs from the existing pinned provider input."""

from verify_urban_roofs import sources as original


def sources():
    return [(row | {"layer": "building"}, []) for row in original()]
