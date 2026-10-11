"""Metric plans, exact mesh sections and spatial cutaways of the same first-storey geometry."""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection
from matplotlib.patches import PathPatch
from matplotlib.path import Path
import numpy as np

from building_layout import polygons
from building_mesh import slice_triangles


COLOURS = ['#d1c5b0', '#555b63', '#b0b6c0', '#3e99b6', '#cbd2d6']


def patch(axis, geometry, **style):
    for polygon in polygons(geometry):
        rings = [polygon.exterior]+list(polygon.interiors)
        coordinates = np.concatenate([np.asarray(ring.coords) for ring in rings])
        codes = np.concatenate([[Path.MOVETO]+[Path.LINETO]*(len(ring.coords)-2)+[Path.CLOSEPOLY] for ring in rings])
        axis.add_patch(PathPatch(Path(coordinates, codes), **style))


def groundplan(axis, plan, product):
    patch(axis, plan['footprint'], facecolor='#edf0f3', edgecolor='#79818a', linewidth=1.)
    for index, room in enumerate(plan['rooms']):
        kind = plan.get('room_kinds', ['generic-room']*len(plan['rooms']))[index]
        colour = '#b7d9c2' if kind == 'corridor' else '#ebd4ae' if kind.endswith('reservation') else plt.get_cmap('Pastel2')(index%8)
        patch(axis, room, facecolor=colour, edgecolor='none')
        point = room.representative_point()
        name = {'corridor': 'Flur', 'stair-reservation': 'Treppenreserve', 'service-reservation': 'Technikreserve'}.get(kind, f'R{index+1}')
        axis.text(point.x, point.y, f'{name}\n{room.area:.0f} m²', ha='center', va='center', fontsize=8)
    if product is not None:
        outlines = slice_triangles(product, [0, 0, 1], plan['floor_z_m']+.40, materials=[1])
        axis.add_collection(LineCollection([points[:, :2] for points, _ in outlines], colors='#242b32', linewidths=1.2))
        for window in plan['windows']:
            patch(axis, window['polygon'], facecolor='#80bfd5', edgecolor='none', alpha=.7)
        patch(axis, plan['entrance']['landing'], facecolor='#cbd2d6', edgecolor='#789282', linewidth=1.)
        for door in plan['doors']:
            axis.plot(*np.asarray(door['centre']), 'o', color='#b76e39', markersize=3)
        centre = np.asarray(plan['entrance']['centre'])
        tangent = np.asarray(plan['entrance']['tangent'])
        inward = np.array([-tangent[1], tangent[0]])
        axis.annotate('', xy=centre+inward*.9, xytext=centre-inward*1.0,
                      arrowprops=dict(arrowstyle='->', color='#28714d', lw=2.))
    axis.autoscale_view()
    axis.set_aspect('equal')
    axis.set_xlabel('Ost [m]')
    axis.set_ylabel('Nord [m]')
    axis.grid(alpha=.15)
    axis.set_title('2D · Grundriss / Wandquerschnitt +0,40 m', fontsize=11)


def section(axis, plan, product):
    tangent = np.asarray(plan['entrance']['tangent'])
    inward = np.array([-tangent[1], tangent[0]])
    centre = np.asarray(plan['entrance']['centre'])
    pieces = slice_triangles(product, [*tangent, 0.], centre@tangent)
    for points, material in pieces:
        axis.plot((points[:, :2]-centre)@inward, points[:, 2], color=COLOURS[material], linewidth=1.5)
    axis.set_ylim(plan['base_z_m']-.25, plan['ceiling_z_m']+.43)
    axis.set_xlabel('Abstand von der Eingangsfassade [m]')
    axis.set_ylabel('Höhe [m]')
    axis.grid(alpha=.15)
    axis.set_title('2D · tatsächlicher Mesh-Schnitt durch den Eingang', fontsize=11)


def figure(sample, plan, product, path, spatial_path=None):
    fig = plt.figure(figsize=(16, 10), dpi=100, facecolor='white')
    grid = fig.add_gridspec(2, 2, width_ratios=[1.1, 1.], left=.06, right=.96, top=.89, bottom=.16, hspace=.42, wspace=.24)
    groundplan(fig.add_subplot(grid[:, 0]), plan, product)
    if product is not None:
        section(fig.add_subplot(grid[0, 1]), plan, product)
        axis = fig.add_subplot(grid[1, 1])
        axis.imshow(plt.imread(spatial_path))
        axis.axis('off')
        axis.set_title('2.5D · tatsächliches Mesh, Geschossdecke ausgeblendet', fontsize=11)
        subtitle = f"{len(plan['rooms'])} Räume · {len(plan['doors'])} offene Türportale · {len(plan['windows'])} Raumfenster · {plan['method']}"
        if plan.get('circulation_status') == 'common-access':
            subtitle = f"{len(plan['rooms'])-2} Nutzflächen + Flur + Kernreserve · {len(plan['doors'])} offene Türportale · {plan['method']}"
    else:
        axis = fig.add_subplot(grid[:, 1])
        axis.axis('off')
        axis.text(.05, .65, 'Kein begehbarer Erdgeschossplan\n\n'+'\n'.join(plan['omissions']), transform=axis.transAxes,
                  fontsize=13, va='top', wrap=True, color='#a44439')
        subtitle = 'Quelle erhalten; unbrauchbare Innenräume werden ausgewiesen'
    stage = 'gemeinsamer Flur und Kernreserve' if plan.get('circulation_status') == 'common-access' else 'funktionaler Gebäudeplan / erster Ausbauschritt'
    fig.suptitle(sample['label']+' · '+stage, fontsize=17, y=.97)
    fig.text(.06, .925, subtitle, fontsize=12)
    fig.text(.06, .09, f"Quellhülle {plan['base_z_m']:.2f}–{plan['source_top_m']:.2f} m; Innenräume bisher nur im ersten Geschoss. "
             'Wand 0,24 m · Trennwand 0,12 m · Tür 1,10 × 2,15 m.', fontsize=10)
    limitations = '; '.join(plan['omissions'])
    fig.text(.06, .06, limitations, fontsize=8, color='#745744', wrap=True)
    fig.savefig(path, dpi=100)
    plt.close(fig)
