"""Generate the cached bevelled title geometry from Noto Sans CJK JP outlines.

Optional authoring tool: requires matplotlib, Shapely >= 2.1, and a JP font.
recover_models.py uses its checked-in output and needs only the standard library.
"""
from pathlib import Path
import argparse
import json
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties
from shapely import constrained_delaunay_triangles
from shapely.geometry import Polygon, GeometryCollection, Point
from shapely.affinity import translate, scale
from shapely.geometry.polygon import orient


def build(font, output):
    text = "回転の国の探検隊"
    outlines = TextPath((0,0), text, size=100, prop=FontProperties(fname=str(font))).to_polygons()
    shape = GeometryCollection()
    # Font outlines contain clockwise contours and counter-clockwise holes.
    # Symmetric difference preserves every counter without relying on font winding.
    for contour in outlines:
        polygon = Polygon(contour)
        if polygon.area > 1e-7:
            shape = shape.symmetric_difference(polygon)
    lo_x,lo_y,hi_x,hi_y = shape.bounds
    shape = translate(shape, -(lo_x+hi_x)/2, -(lo_y+hi_y)/2)
    shape = scale(shape, xfact=12/(hi_x-lo_x), yfact=12/(hi_x-lo_x), origin=(0,0))
    inset = shape.buffer(-.018, join_style=2)
    bevel = shape.difference(inset)
    triangles = []

    def surface(geometry, front, bevelled=False):
        for triangle in constrained_delaunay_triangles(geometry).geoms:
            vertices = list(orient(triangle, sign=1).exterior.coords)[:3]
            points = []
            for x,y in vertices:
                z = .32 if front else -.32
                if bevelled:
                    z = .27 if shape.boundary.distance(Point(x,y)) < 1e-7 else .32
                    if not front: z = -z
                points.append([round(x,6),round(y,6),round(z,6)])
            triangles.append(points if front else points[::-1])

    surface(inset, True)
    surface(inset, False)
    surface(bevel, True, True)
    surface(bevel, False, True)
    for polygon in shape.geoms if hasattr(shape,"geoms") else [shape]:
        polygon = orient(polygon, sign=1)
        for ring in [polygon.exterior, *polygon.interiors]:
            points = list(ring.coords)
            for a,b in zip(points, points[1:]):
                # Outer contour and hole contour have opposite winding.
                quad = [[*a,-.27],[*b,-.27],[*b,.27],[*a,.27]]
                triangles.extend([[quad[0],quad[1],quad[2]],[quad[0],quad[2],quad[3]]])
    data = {"title":text,"source_font":"Noto Sans CJK JP Bold","depth":.64,
            "front_area":round(shape.area,6),"triangles":triangles}
    output.write_text(json.dumps(data, ensure_ascii=False, separators=(",",":"))+"\n",encoding="utf-8")
    print(f"Bevelled title: {len(triangles)} triangles, .64 depth, {shape.area:.4f} front area")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("title_mesh.json"))
    args = parser.parse_args()
    build(args.font, args.output)
