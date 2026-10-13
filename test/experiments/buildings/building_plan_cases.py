"""Small constructive counterexamples and the same pinned footprints as the Esri oracle."""

import shapely

from samples import read_site


def cases():
    outlines = [
        ('rectangle', shapely.box(0, 0, 14, 10), 0., 3.),
        ('L-wing', shapely.Polygon([(0, 0), (18, 0), (18, 7), (7, 7), (7, 16), (0, 16)]), 0., 6.),
        ('courtyard', shapely.Polygon([(0, 0), (26, 0), (26, 22), (0, 22)],
                                    [[(7, 6), (7, 16), (19, 16), (19, 6)]]), 0., 9.),
        ('thin-wing', shapely.box(0, 0, 10, 1.05), 0., 3.),
        ('narrow-neck', shapely.union_all([shapely.box(0, 0, 6, 6), shapely.box(6, 2.6, 12, 3.4),
                                          shapely.box(12, 0, 18, 6)]), 0., 3.),
        ('raised', shapely.box(0, 0, 8, 7), 1.5, 7.5),
        ('underground', shapely.box(0, 0, 8, 7), -1.2, 4.8),
        ('low-ceiling', shapely.box(0, 0, 8, 7), 0., 1.8),
    ]
    rows = [dict(id=name, label=name, polygon=polygon,
                 properties=dict(render_min_height=bottom, render_height=top), source_sha256=None)
            for name, polygon, bottom, top in outlines]
    for site, identifiers in [('Vienna', ['Vienna:1511580:38', 'Vienna:1565540:9']),
                              ('Tokyo', ['Tokyo:1019257600:10'])]:
        samples, _ = read_site(site, context=True)
        for identifier in identifiers:
            sample = next(row for row in samples if row['id'] == identifier)
            sample['label'] = 'vienna-compact' if identifier.endswith('1511580:38') else (
                'vienna-courtyard' if site == 'Vienna' else 'tokyo-compact')
            rows.append(sample)
    return rows
