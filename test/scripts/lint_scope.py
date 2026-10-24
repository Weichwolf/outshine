"""Select changed files and their native consumers from the actual include contexts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess

SPEC = importlib.util.spec_from_file_location('layer_contract',
                                             Path(__file__).with_name('layer-contract.py'))
LAYERS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LAYERS)
CPP = {'.cpp', '.mm', '.h', '.hpp'}
RULES = {'Makefile', '.clang-format', '.clang-tidy', 'test/run.sh', 'test/lint.sh',
         'test/lint-changed.sh', 'test/strip-comments.py'}


def changed_paths(root, base):
    diff = subprocess.check_output(['git', 'diff', '--name-only', '-z', base, '--'], cwd=root)
    untracked = subprocess.check_output(
        ['git', 'ls-files', '--others', '--exclude-standard', '-z'], cwd=root)
    return {Path(os.fsdecode(name)) for name in (diff + untracked).split(b'\0') if name}


def full_reasons(paths):
    return sorted(str(path) for path in paths
                  if str(path) in RULES or path.name in {'reaches', 'Makefile', '.clang-format', '.clang-tidy'}
                  or path.suffix == '.mk'
                  or path.parts[:2] in {('test', 'scripts'), ('test', 'harness')}
                  or path.parts[:3] == ('src', 'render', 'shaders')
                  or path.suffix in {'.glsl', '.vert', '.frag', '.comp', '.geom', '.tesc', '.tese'})


def affected_units(root, records, changed):
    paths = {root / path for path in changed}
    headers = {path for path in paths if path.suffix in {'.h', '.hpp'}}
    if any(not header.is_file() for header in headers):
        return sorted(Path(entry['file']).resolve() for entry in records)
    reader = LAYERS.HeaderDependencies(root)
    selected = []
    for entry in records:
        source = Path(entry['file'])
        if not source.is_absolute():
            source = Path(entry['directory']) / source
        source = source.resolve()
        if source in paths or (headers and
                              headers.intersection(reader.read(source,
                                                               LAYERS.include_directories(entry)))):
            selected.append(source)
    return sorted(selected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--base', default='HEAD')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    from tidy_analysis import database_units
    database_units(root)
    changed = changed_paths(root, args.base)
    reasons = full_reasons(changed)
    records = json.loads((root / 'compile_commands.json').read_text())
    units = affected_units(root, records, changed)
    files = sorted(str(path) for path in changed if path.suffix in CPP
                   and path.parts[0] in {'src', 'test'} and (root / path).is_file()
                   and path.parts[:3] != ('src', 'render', 'shaders'))
    plan = {'base': args.base, 'full': bool(reasons), 'reasons': reasons,
            'changed': sorted(map(str, changed)), 'format': files,
            'units': [str(path) for path in units]}
    args.output.write_text(json.dumps(plan, indent=2) + '\n')
    print(f'lint scope: {len(files)} changed C++ files; {len(units)}/{len(records)} native units; '
          + ('full integration required' if reasons else 'changed scope'))


if __name__ == '__main__':
    main()
