"""The production translation units selected for one host platform."""

from pathlib import Path
import platform


def source_units(root, system=None):
    system = platform.system() if system is None else system
    selected = []
    for path in (Path(root) / 'src').rglob('*'):
        if not path.is_file() or path.suffix not in ('.cpp', '.mm'):
            continue
        if path.suffix == '.mm' and system != 'Darwin':
            continue
        parts = path.relative_to(root).parts
        if parts[:4] == ('src', 'render', 'device', 'native') and len(parts) > 5:
            if parts[4] == 'metal' and system != 'Darwin':
                continue
            if parts[4] == 'vulkan' and system != 'Linux':
                continue
        selected.append(path.resolve())
    return sorted(selected)


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[2]
    for unit in source_units(root):
        print(unit.relative_to(root).as_posix())
