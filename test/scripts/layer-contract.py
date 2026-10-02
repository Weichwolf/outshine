"""Check declared tier cycles and actual compiler include access, not build spelling."""
import json
import pathlib
import re
import shlex


PUBLIC_TIERS = {
    'Earth.h': 'base',
    'Extent.h': 'base',
    'Logging.h': 'base',
    'Outshine.h': 'engine',
    'audio': 'audio',
    'diagnostics': 'diagnostics',
    'export': 'import',
    'generation': 'generators',
    'import': 'import',
    'math': 'base',
    'physics': 'actor',
    'render': 'render',
    'scenario': 'scenario',
    'scene': 'base',
    'world': 'world',
}

KNOWN_PUBLIC_FINDINGS = {}


def physical_build_includes(text):
    expression = r'^\s*#\s*include\s*[<"]([^>"]+)[>"]'
    return [name for name in re.findall(expression, text, re.MULTILINE)
            if 'build' in pathlib.PurePosixPath(name).parts]


def parent_includes(text):
    expression = r'^\s*#\s*include\s*[<"]([^>"]+)[>"]'
    return [name for name in re.findall(expression, text, re.MULTILINE)
            if '..' in pathlib.PurePosixPath(name).parts]


def absolute_includes(text):
    expression = r'^\s*#\s*include\s*[<"]([^>"]+)[>"]'
    return [name for name in re.findall(expression, text, re.MULTILINE)
            if pathlib.PurePosixPath(name).is_absolute()]


def allowed(graph, owner, target):
    return target == owner or owner.startswith(target + '/') or target in graph[owner]


def errors(graph, commands, public_edges=()):
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
        accessible = [owner, *graph[owner]]
        for directory in includes:
            if not any(directory == tier or directory.startswith(tier + '/')
                       for tier in accessible):
                found.append(f"{source}: undeclared include access {directory}")
    for source, target in public_edges:
        owners = [tier for tier in graph if source.startswith(tier + '/')]
        if not owners:
            found.append(f"no tier for {source}")
            continue
        owner = max(owners, key=len)
        if target not in graph:
            found.append(f"{source}: public header belongs to unknown tier {target}")
        elif not allowed(graph, owner, target):
            found.append(f"{source}: undeclared public-header dependency {target}")
    return found


def include_directories(entry):
    args = entry.get('arguments') or shlex.split(entry['command'])
    directories = []
    for i, arg in enumerate(args):
        if arg == '-I':
            value = args[i + 1]
        elif arg.startswith('-I'):
            value = arg[2:]
        else:
            continue
        directories.append((pathlib.Path(entry['directory']) / value).resolve())
    return directories


def public_owner(header, include):
    relative = header.relative_to(include)
    key = relative.parts[0]
    if key not in PUBLIC_TIERS:
        raise RuntimeError(f'unclassified public header {relative}')
    return PUBLIC_TIERS[key]


def header_dependencies(source, directories, root):
    expression = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
    pending = [source]
    visited = set()
    while pending:
        current = pending.pop()
        if current in visited:
            continue
        visited.add(current)
        if not current.is_relative_to(root) or current.suffix not in {'.h', '.hpp', '.cpp'}:
            continue
        for named in expression.findall(current.read_text(errors='replace')):
            candidates = [current.parent / named, *(directory / named for directory in directories)]
            resolved = next((candidate.resolve() for candidate in candidates if candidate.is_file()), None)
            if resolved is None or not resolved.is_relative_to(root):
                continue
            pending.append(resolved)
    return visited


def native_product_boundary(headers):
    allowed_private = ('src/world/products/', 'src/base/', 'src/content/')
    return [f'{header}: native world product depends on a non-product implementation'
            for header in sorted(headers)
            if header.startswith('src/') and not header.startswith(allowed_private)]


def world_state_boundary(headers):
    return [f'{header}: world state depends on engine, generator or renderer implementation'
            for header in sorted(headers)
            if header.startswith(('src/engine/', 'src/generators/', 'src/render/'))]


def generator_product_boundary(headers):
    controllers = ('src/engine/',)
    return [f'{header}: generator depends on classification or engine orchestration'
            for header in sorted(headers) if header.startswith(controllers)]


def main():
    good = {'base': [], 'render': ['base'], 'world': ['base']}
    controls = [
        physical_build_includes('#include "../../build/CrownBuild.h"') ==
        ['../../build/CrownBuild.h'],
        not physical_build_includes('#include "CrownBuild.h"'),
        not physical_build_includes('#include "../../world/sky/AtmosphereCore.h"'),
        parent_includes('#include "../../world/sky/AtmosphereCore.h"') ==
        ['../../world/sky/AtmosphereCore.h'],
        not parent_includes('#include "world/sky/AtmosphereCore.h"'),
        absolute_includes('#include "/tmp/generated/Config.h"') ==
        ['/tmp/generated/Config.h'],
        not absolute_includes('#include "OutshineGenerated/CrownBuildIdentity.h"'),
        not errors(good, [('render/Draw.cpp', ['render', 'base/math'])]),
        bool(errors(good, [('render/Draw.cpp', ['world'])])),
        bool(errors({'a': ['b'], 'b': ['c'], 'c': ['a']}, [])),
        bool(errors({'a': ['missing']}, [])),
        bool(errors(good, [('unknown/Thing.cpp', [])])),
        bool(errors(good, [('render/Draw.cpp', ['baseball'])])),
        not errors(good, [], [('render/Draw.cpp', 'base')]),
        bool(errors(good, [], [('render/Draw.cpp', 'world')])),
    ]
    if not all(controls):
        raise RuntimeError('layer oracle positive/negative controls failed')
    root = pathlib.Path.cwd()
    src = root / 'src'
    include = root / 'include'
    graph = {str(p.parent.relative_to(src)): p.read_text().split()
             for p in src.rglob('reaches')}
    commands = []
    public_edges = set()
    generator_violations = []
    for entry in json.loads((root / 'compile_commands.json').read_text()):
        source = pathlib.Path(entry['file']).resolve()
        if not source.is_relative_to(src):
            continue
        directories = include_directories(entry)
        includes = []
        for directory in directories:
            if directory.is_relative_to(src):
                includes.append(str(directory.relative_to(src)))
        commands.append((str(source.relative_to(src)), includes))
        dependencies = header_dependencies(source, directories, root)
        for target in {public_owner(header, include)
                       for header in dependencies if header.is_relative_to(include)}:
            public_edges.add((str(source.relative_to(src)), target))
        if source.is_relative_to(src / 'generators'):
            generator_violations.extend(generator_product_boundary(
                {str(header.relative_to(root)) for header in dependencies
                 if header.is_relative_to(root)}))
    if not graph or not commands:
        raise RuntimeError('missing tier graph or compilation commands')
    violations = errors(graph, commands, public_edges) + generator_violations
    if native_product_boundary({'src/world/products/BuildingGeometry.h', 'src/base/math/Capacity.h'}):
        raise RuntimeError('native product positive control failed')
    if not native_product_boundary({'src/world/data/OsmElements.h'}):
        raise RuntimeError('native product negative control failed')
    if world_state_boundary({'src/world/products/BuildingGeometry.h', 'src/base/io/Tasks.h'}):
        raise RuntimeError('world state positive control failed')
    if not world_state_boundary({'src/engine/streaming/SurfacePreparation.h'}):
        raise RuntimeError('world state negative control failed')
    if not world_state_boundary({'src/generators/terrain/ClassificationRasterizer.h'}):
        raise RuntimeError('world generator negative control failed')
    if not world_state_boundary({'src/render/device/GroundClassBuffer.h'}):
        raise RuntimeError('world renderer negative control failed')
    if generator_product_boundary({'src/world/ground/ClassStructure.h'}):
        raise RuntimeError('generator product positive control failed')
    for controller in ('src/engine/streaming/ClassificationPreparation.h',
                       'src/engine/streaming/ClassificationBuild.h',
                       'src/engine/streaming/SurfacePreparation.h'):
        if not generator_product_boundary({controller}):
            raise RuntimeError('generator product negative control failed')
    directories = sorted({directory for entry in json.loads(
        (root / 'compile_commands.json').read_text()) for directory in include_directories(entry)})
    for product in (src / 'world' / 'products').glob('*.h'):
        dependencies = header_dependencies(product, directories, root)
        violations.extend(native_product_boundary(
            {str(header.relative_to(root)) for header in dependencies if header.is_relative_to(root)}))
    for source in (src / 'world').rglob('*'):
        if source.suffix not in {'.h', '.cpp'}:
            continue
        dependencies = header_dependencies(source, directories, root)
        violations.extend(world_state_boundary(
            {str(header.relative_to(root)) for header in dependencies if header.is_relative_to(root)}))
    for tree in (src, include, root / 'test'):
        for source in tree.rglob('*'):
            if source.suffix not in {'.h', '.hpp', '.cpp', '.inc', '.glsl', '.vert', '.frag', '.comp'}:
                continue
            text = source.read_text()
            for name in physical_build_includes(text):
                violations.append(f'{source.relative_to(root)}: physical build include {name}')
            for name in parent_includes(text):
                violations.append(f'{source.relative_to(root)}: parent-directory include {name}')
            for name in absolute_includes(text):
                violations.append(f'{source.relative_to(root)}: absolute include {name}')
    known = {
        f'{source}: undeclared public-header dependency {target}': wi
        for (source, target), wi in KNOWN_PUBLIC_FINDINGS.items()
    }
    failures = [failure for failure in violations if failure not in known]
    stale = sorted(set(known) - set(violations))
    failures.extend(f'stale known finding: {finding}' for finding in stale)
    present_known = set(violations) & set(known)
    for finding in sorted(present_known):
        print(f'KNOWN WI {known[finding]}: {finding}')
    for failure in failures:
        print(failure)
    print(f'{len(graph)} tiers, {len(commands)} compile commands, '
          f'{len(public_edges)} public edges, {len(present_known)} known findings, '
          f'{len(failures)} violations')
    print('NOT COVERED: preprocessor conditionals, runtime coupling')
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
