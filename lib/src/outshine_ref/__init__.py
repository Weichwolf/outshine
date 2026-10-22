"""Reference contracts and procedures loaded when their consumers request them."""

from importlib import import_module

_EXPORTS = {
    "contracts": (
        "BuildingFootprint",
        "FootprintAdjustment",
        "HeightInterval",
        "Hint",
        "LocalFrame",
        "Mesh",
        "Reservation",
        "SourceFeature",
        "SourceInputs",
    ),
    "pipeline": ("PreparedWorld", "prepare"),
}
_MODULES = ("buildings", "geometry", "transport", "vegetation")
__all__ = [name for names in _EXPORTS.values() for name in names] + list(_MODULES)


def __getattr__(name):
    if name in _MODULES or name in _EXPORTS:
        value = import_module(f".{name}", __name__)
    else:
        owner = next(
            (module for module, names in _EXPORTS.items() if name in names), None
        )
        if owner is None:
            raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
        value = getattr(import_module(f".{owner}", __name__), name)
    globals()[name] = value
    return value


def __dir__():
    return sorted(set(globals()) | set(__all__) | set(_EXPORTS))
