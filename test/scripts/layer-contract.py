"""Check declared tier cycles and actual compiler include access, not build spelling."""

import json
import pathlib
import re
import shlex


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
    return (target == owner or target.startswith(owner + '/') or owner.startswith(target + '/') or
            any(target == tier or target.startswith(tier + '/') for tier in graph[owner]))


def errors(graph, commands, header_edges=()):
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
    for source, target in header_edges:
        owners = [tier for tier in graph if source.startswith(tier + '/')]
        if not owners:
            found.append(f"no tier for {source}")
            continue
        owner = max(owners, key=len)
        if target not in graph:
            found.append(f"{source}: header belongs to unknown tier {target}")
        elif not allowed(graph, owner, target):
            found.append(f"{source}: undeclared header dependency {target}")
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


def header_owner(header, src, graph):
    relative = str(header.relative_to(src))
    owners = [tier for tier in graph if relative.startswith(tier + '/')]
    if not owners:
        raise RuntimeError(f'unclassified component header {relative}')
    return max(owners, key=len)


class HeaderDependencies:
    """Traverse one frozen snapshot while preserving each ordered include context."""

    def __init__(self, root):
        self.root = root
        self.include_names = {}
        self.resolved_paths = {}
        self.include_edges = {}

    def resolve(self, candidate):
        if candidate not in self.resolved_paths:
            self.resolved_paths[candidate] = candidate.resolve() if candidate.is_file() else None
        return self.resolved_paths[candidate]

    def direct(self, current, directories):
        key = current, directories
        if key not in self.include_edges:
            if current not in self.include_names:
                self.include_names[current] = re.findall(
                    r'^\s*#\s*include\s*[<"]([^>"]+)[>"]',
                    current.read_text(errors='replace'), re.MULTILINE)
            found = []
            for name in self.include_names[current]:
                for directory in (current.parent, *directories):
                    resolved = self.resolve(directory / name)
                    if resolved is not None:
                        if resolved.is_relative_to(self.root):
                            found.append(resolved)
                        break
            self.include_edges[key] = tuple(found)
        return self.include_edges[key]

    def read(self, source, directories):
        directories = tuple(directories)
        pending = [source]
        visited = set()
        while pending:
            current = pending.pop()
            if current in visited:
                continue
            visited.add(current)
            if not current.is_relative_to(self.root) or current.suffix not in {'.h', '.hpp', '.cpp', '.mm'}:
                continue
            pending.extend(self.direct(current, directories))
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


def transport_product_boundary(headers):
    allowed_private = ('src/world/navigation/', 'src/world/products/', 'src/base/', 'src/content/')
    return [f'{header}: native transport product depends on source interpretation or orchestration'
            for header in sorted(headers)
            if header.startswith('src/') and not header.startswith(allowed_private)]


def generator_product_boundary(headers):
    controllers = ('src/engine/',)
    return [f'{header}: generator depends on classification or engine orchestration'
            for header in sorted(headers) if header.startswith(controllers)]


def main():
    good = {'base': [], 'render': ['base'], 'world': ['base']}
    controls = [
        physical_build_includes('#include "../../build/GeneratedConfig.h"') ==
        ['../../build/GeneratedConfig.h'],
        not physical_build_includes('#include "GeneratedConfig.h"'),
        not physical_build_includes('#include "../../world/sky/AtmosphereCore.h"'),
        parent_includes('#include "../../world/sky/AtmosphereCore.h"') ==
        ['../../world/sky/AtmosphereCore.h'],
        not parent_includes('#include "world/sky/AtmosphereCore.h"'),
        absolute_includes('#include "/tmp/generated/Config.h"') ==
        ['/tmp/generated/Config.h'],
        not absolute_includes('#include "OutshineGenerated/Config.h"'),
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
    graph = {str(p.parent.relative_to(src)): p.read_text().split()
             for p in src.rglob('reaches')}
    reader = HeaderDependencies(root)
    commands = []
    header_edges = set()
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
        dependencies = reader.read(source, directories)
        for target in {header_owner(header, src, graph)
                       for header in dependencies if header.is_relative_to(src) and header.suffix in {'.h', '.hpp'}}:
            header_edges.add((str(source.relative_to(src)), target))
        if source.is_relative_to(src / 'generators'):
            generator_violations.extend(generator_product_boundary(
                {str(header.relative_to(root)) for header in dependencies
                 if header.is_relative_to(root)}))
    if not graph or not commands:
        raise RuntimeError('missing tier graph or compilation commands')
    violations = errors(graph, commands, header_edges) + generator_violations
    if native_product_boundary({'src/world/products/BuildingGeometry.h', 'src/base/math/Capacity.h'}):
        raise RuntimeError('native product positive control failed')
    if not native_product_boundary({'src/generators/osm/import/OsmElementSet.h'}):
        raise RuntimeError('native product negative control failed')
    if world_state_boundary({'src/world/products/BuildingGeometry.h', 'src/base/io/Tasks.h'}):
        raise RuntimeError('world state positive control failed')
    if not world_state_boundary({'src/engine/streaming/SurfacePreparation.h'}):
        raise RuntimeError('world state negative control failed')
    if not world_state_boundary({'src/generators/terrain/ClassificationRasterizer.h'}):
        raise RuntimeError('world generator negative control failed')
    if not world_state_boundary({'src/generators/osm/import/OsmXmlReader.h'}):
        raise RuntimeError('world source interpretation negative control failed')
    if not world_state_boundary({'src/render/terrain/PackedTerrainClassification.h'}):
        raise RuntimeError('world renderer negative control failed')
    if generator_product_boundary({'src/world/ground/TerrainClassificationMap.h'}):
        raise RuntimeError('generator product positive control failed')
    for controller in ('src/engine/streaming/ClassificationPreparation.h',
                       'src/engine/streaming/ClassificationBuild.h',
                       'src/engine/streaming/SurfacePreparation.h'):
        if not generator_product_boundary({controller}):
            raise RuntimeError('generator product negative control failed')
    directories = sorted({directory for entry in json.loads(
        (root / 'compile_commands.json').read_text()) for directory in include_directories(entry)})
    for product in (src / 'world' / 'products').glob('*.h'):
        dependencies = reader.read(product, directories)
        violations.extend(native_product_boundary(
            {str(header.relative_to(root)) for header in dependencies if header.is_relative_to(root)}))
    if transport_product_boundary({'src/world/navigation/TransportTopology.h', 'src/world/products/SourceIdentity.h'}):
        raise RuntimeError('native transport positive control failed')
    for forbidden in ('src/generators/osm/import/OsmElementSet.h', 'src/generators/osm/transport/OsmTransportBuilder.h',
                      'src/generators/osm/preparation/OsmTransportPreparation.h'):
        if not transport_product_boundary({forbidden}):
            raise RuntimeError('native transport negative control failed')
    for product in (src / 'world' / 'navigation').glob('*.h'):
        dependencies = reader.read(product, directories)
        violations.extend(transport_product_boundary(
            {str(header.relative_to(root)) for header in dependencies if header.is_relative_to(root)}))
    for source in (src / 'world').rglob('*'):
        if source.suffix not in {'.h', '.cpp'}:
            continue
        dependencies = reader.read(source, directories)
        violations.extend(world_state_boundary(
            {str(header.relative_to(root)) for header in dependencies if header.is_relative_to(root)}))
    for tree in (src, root / 'test'):
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
    failures = sorted(set(violations))
    for failure in failures:
        print(failure)
    print(f'{len(graph)} tiers, {len(commands)} compile commands, '
          f'{len(header_edges)} component header edges, '
          f'{len(failures)} violations')
    print('NOT COVERED: preprocessor conditionals, runtime coupling')
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
