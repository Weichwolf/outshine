"""Resolve each corpus's input-fetching adapter from its matching harness directory."""
import importlib.util
import os

_LOADED = {}

def _repository(start):
    at = os.path.abspath(start)
    while at != os.path.dirname(at):
        if os.path.isdir(os.path.join(at, "test", "harness")):
            return at
        at = os.path.dirname(at)
    raise RuntimeError("no parent of " + start + " holds test/harness")

def harness_root(start):
    """`test/harness/` itself, found from anywhere inside it."""
    return os.path.join(_repository(start), "test", "harness")

def harness_of(case_directory):
    """The harness directory serving the corpus this case belongs to."""
    root = _repository(case_directory)
    relative = os.path.relpath(os.path.abspath(case_directory), os.path.join(root, "test"))
    parts = relative.split(os.sep)
    found = None
    for depth in range(1, len(parts) + 1):
        candidate = os.path.join(root, "test", "harness", *parts[:depth])
        if os.path.isdir(candidate):
            found = candidate
    if found is None:
        raise RuntimeError("no harness under test/harness/ serves " + case_directory)
    return found

def beside(anchor, name):
    """The module named `name` sitting beside `anchor`, loaded by path rather than by package."""
    return at(os.path.join(os.path.dirname(os.path.abspath(anchor)), name + ".py"))

def step(case_directory, name):
    """The named step of the harness serving this case, or None where that harness declares none."""
    path = os.path.join(harness_of(case_directory), "prepare", name + ".py")
    return at(path) if os.path.isfile(path) else None

def at(path):
    path = os.path.abspath(path)
    if path in _LOADED:
        return _LOADED[path]
    specification = importlib.util.spec_from_file_location(
        "outshine_vendor_" + os.path.basename(path)[:-3], path)
    module = importlib.util.module_from_spec(specification)
    _LOADED[path] = module
    specification.loader.exec_module(module)
    return module
