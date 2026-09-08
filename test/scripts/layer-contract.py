"""Check declared tier cycles and actual compiler include access, not build spelling."""
import json
import pathlib
import shlex


def errors(graph, commands):
    found = []
    visiting, visited = set(), set()

    def visit(node):
        if node in visiting:
            found.append(f"cycle through {node}")
            return
        if node in visited:
            return
        visiting.add(node)
        for target in graph[node]:
            if target not in graph:
                found.append(f"unknown tier {node} -> {target}")
            else:
                visit(target)
        visiting.remove(node)
        visited.add(node)

    for node in graph:
        visit(node)
    for source, includes in commands:
        owners = [tier for tier in graph if source.startswith(tier + '/')]
        if not owners:
            found.append(f"no tier for {source}")
            continue
        owner = max(owners, key=len)
        allowed = [owner, *graph[owner]]
        for directory in includes:
            if not any(directory == tier or directory.startswith(tier + '/') for tier in allowed):
                found.append(f"{source}: undeclared include access {directory}")
    return found


def main():
    good = {'base': [], 'render': ['base'], 'world': ['base']}
    controls = [
        not errors(good, [('render/Draw.cpp', ['render', 'base/math'])]),
        bool(errors(good, [('render/Draw.cpp', ['world'])])),
        bool(errors({'a': ['b'], 'b': ['c'], 'c': ['a']}, [])),
        bool(errors({'a': ['missing']}, [])),
        bool(errors(good, [('unknown/Thing.cpp', [])])),
        bool(errors(good, [('render/Draw.cpp', ['baseball'])])),
    ]
    if not all(controls):
        raise RuntimeError('layer oracle positive/negative controls failed')
    root = pathlib.Path.cwd()
    src = root / 'src'
    graph = {str(p.parent.relative_to(src)): p.read_text().split()
             for p in src.rglob('reaches')}
    commands = []
    for entry in json.loads((root / 'compile_commands.json').read_text()):
        source = pathlib.Path(entry['file']).resolve()
        if not source.is_relative_to(src):
            continue
        args = entry.get('arguments') or shlex.split(entry['command'])
        includes = []
        for i, arg in enumerate(args):
            if arg == '-I':
                value = args[i + 1]
            elif arg.startswith('-I'):
                value = arg[2:]
            else:
                continue
            directory = (pathlib.Path(entry['directory']) / value).resolve()
            if directory.is_relative_to(src):
                includes.append(str(directory.relative_to(src)))
        commands.append((str(source.relative_to(src)), includes))
    if not graph or not commands:
        raise RuntimeError('missing tier graph or compilation commands')
    failures = errors(graph, commands)
    for failure in failures:
        print(failure)
    print(f'{len(graph)} tiers, {len(commands)} compile commands, {len(failures)} violations')
    print('NOT COVERED: preprocessor conditionals, relative-include bypasses, runtime coupling')
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
