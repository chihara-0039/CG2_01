"""Lost OBJ recovery: create simple, triangulated meshes without external packages.

Existing non-recovery models are never overwritten. All coordinates are authored
in OBJ space; Model::LoadObjFile applies the engine's usual Z conversion.
The lettering is geometry, since the old title/clear textures are solid colors.
"""
from pathlib import Path
import argparse
import math
import json
import shutil

ROOT = Path(__file__).resolve().parents[1] / "Resources"
MARKER = "# Recovery placeholder OBJ; not the original artwork."
# Original 5x7 block lettering, sufficient for readable recovery UI.
FONT = {
    "A": [14,17,17,31,17,17,17], "C": [14,17,16,16,16,17,14],
    "E": [31,16,16,30,16,16,31], "F": [31,16,16,30,16,16,16],
    "G": [14,17,16,23,17,17,14], "I": [31,4,4,4,4,4,31],
    "L": [16,16,16,16,16,16,31], "N": [17,25,25,21,19,19,17],
    "O": [14,17,17,17,17,17,14], "P": [30,17,17,30,16,16,16],
    "R": [30,17,17,30,20,18,17], "S": [15,16,16,14,1,1,30],
    "T": [31,4,4,4,4,4,4], "U": [17,17,17,17,17,17,14],
    "D": [30,17,17,17,17,17,30], "Y": [17,17,10,4,4,4,4],
    "X": [17,17,10,4,10,17,17], "B": [30,17,17,30,17,17,30],
    "K": [17,18,20,24,20,18,17], "M": [17,27,21,21,17,17,17],
    "W": [17,17,17,21,21,21,10], "V": [17,17,17,17,17,10,4],
    "0": [14,17,19,21,25,17,14], "1": [4,12,4,4,4,4,14],
    "2": [14,17,1,2,4,8,31], "3": [30,1,1,14,1,1,30],
    "4": [2,6,10,18,31,2,2], "5": [31,16,16,30,1,1,30],
    "6": [14,16,16,30,17,17,14], "7": [31,1,2,4,8,8,8],
    "8": [14,17,17,14,17,17,14], "9": [14,17,17,15,1,1,14],
    "_": [0,0,0,0,0,0,31], "/": [1,2,2,4,8,8,16],
    ":": [0,4,4,0,4,4,0], " ": [0]*7,
}


class Mesh:
    def __init__(self):
        self.lines = [MARKER]
        self.vertex_count = 0

    def triangle(self, a, b, c, uv=((0,0),(1,0),(1,1))):
        ab = [b[i]-a[i] for i in range(3)]
        ac = [c[i]-a[i] for i in range(3)]
        n = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
        length = math.sqrt(sum(v*v for v in n))
        if length < 1e-9:
            return
        n = tuple(v/length for v in n)
        for position, texcoord in zip((a,b,c), uv):
            self.lines.append("v " + " ".join(f"{v:.6f}" for v in position))
            self.lines.append("vt " + " ".join(f"{v:.6f}" for v in texcoord))
            self.lines.append("vn " + " ".join(f"{v:.6f}" for v in n))
        indices = range(self.vertex_count+1, self.vertex_count+4)
        self.lines.append("f " + " ".join(f"{i}/{i}/{i}" for i in indices))
        self.vertex_count += 3

    def box(self, center=(0,0,0), size=(1,1,1), texture=None):
        x,y,z = center
        w,h,depth = (v/2 for v in size)
        points = [(x-w,y-h,z-depth),(x+w,y-h,z-depth),(x+w,y+h,z-depth),(x-w,y+h,z-depth),
                  (x-w,y-h,z+depth),(x+w,y-h,z+depth),(x+w,y+h,z+depth),(x-w,y+h,z+depth)]
        for face, corners in enumerate(((0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(3,7,6,2),(0,1,5,4))):
            a,b,c,d = (points[i] for i in corners)
            uv = ((0,0),(1,0),(1,1),(0,1))
            if texture == "wall":
                # 元画像はキューブ展開図。白い余白を含まないレンガの面を使う。
                uv = ((.26,.51),(.49,.51),(.49,.74),(.26,.74))
            elif texture == "grass":
                if face >= 4:
                    lo, hi = (.72,.98) if face == 4 else (.02,.40)
                    uv = ((lo,.04),(hi,.04),(hi,.96),(lo,.96))
                else:
                    # 元画像は右が草・左が土。高さをUへ割り当て草が上になるようにする。
                    uv = tuple((.10 + .80*(p[1]-(y-h))/size[1],
                                .04 + .92*((p[0]-(x-w))/size[0] if face < 2 else (p[2]-(z-depth))/size[2]))
                               for p in (a,b,c,d))
            elif texture == "stageSelect":
                # OBJのZ反転後の前・右・裏・左・上・下に対応する六面を個別に割り当てる。
                rectangles = ((.625,.25,.875,.50),(.125,.25,.375,.50),
                              (.375,.75,.625,1.0),(.375,.25,.625,.50),
                              (.375,0,.625,.25),(.375,.50,.625,.75))
                left,top,right,bottom = rectangles[face]
                # 各面が選択回転で正面に来た時の、画面上の四頂点位置。
                screen = (((1,1),(1,0),(0,0),(0,1)), ((0,1),(1,1),(1,0),(0,0)),
                          ((0,1),(1,1),(1,0),(0,0)), ((1,1),(1,0),(0,0),(0,1)),
                          ((0,0),(0,1),(1,1),(1,0)), ((0,1),(1,1),(1,0),(0,0)))
                coordinates = []
                for s,t in screen[face]:
                    if face == 2: s,t = 1-s,1-t
                    elif face == 4: s,t = 1-t,s
                    elif face == 5: s,t = t,1-s
                    coordinates.append((left+(right-left)*s, 1-(top+(bottom-top)*t)))
                uv = tuple(coordinates)
            self.triangle(a,b,c,uv[:3])
            self.triangle(a,c,d,(uv[0],uv[2],uv[3]))

    def text(self, text, height=1.4):
        pixel = height/7
        width = (len(text)*6-1)*pixel
        for index, character in enumerate(text):
            for row, mask in enumerate(FONT[character]):
                for column in range(5):
                    if mask & (1 << (4-column)):
                        self.box(((index*6+column+.5)*pixel-width/2,
                                  (3-row)*pixel, 0), (pixel*.94,pixel*.94,.12))

    def japanese_title(self, height=1.5):
        # 輪郭から作ったベベル・側面・裏面を持つ一体の立体文字。
        # 生成済み三角形を保存し、通常の再生成にはフォントや外部ライブラリを不要にする。
        data = json.loads(Path(__file__).with_name("title_mesh.json").read_text(encoding="utf-8"))
        self.lines.append("# Game title: " + data["title"])
        for triangle in data["triangles"]:
            self.triangle(*triangle)

    def sphere(self, radius=.5, rings=8, sectors=16):
        def point(row,col):
            latitude = math.pi*row/rings
            longitude = math.tau*col/sectors
            return (radius*math.sin(latitude)*math.cos(longitude),
                    radius*math.cos(latitude), radius*math.sin(latitude)*math.sin(longitude))
        for row in range(rings):
            for col in range(sectors):
                a,b,c,d = point(row,col),point(row+1,col),point(row+1,col+1),point(row,col+1)
                ua,ub,uc,ud = (col/sectors,row/rings),(col/sectors,(row+1)/rings),((col+1)/sectors,(row+1)/rings),((col+1)/sectors,row/rings)
                self.triangle(a,c,b,(ua,uc,ub))
                self.triangle(a,d,c,(ua,ud,uc))

    def star(self):
        points = [(math.cos(math.pi/2+i*math.pi/5)*(.5 if i%2==0 else .22),
                   math.sin(math.pi/2+i*math.pi/5)*(.5 if i%2==0 else .22)) for i in range(10)]
        for i in range(10):
            a,b = points[i],points[(i+1)%10]
            self.triangle((0,0,.12),(a[0],a[1],.12),(b[0],b[1],.12))
            self.triangle((0,0,-.12),(b[0],b[1],-.12),(a[0],a[1],-.12))
            self.triangle((a[0],a[1],-.12),(b[0],b[1],-.12),(b[0],b[1],.12))
            self.triangle((a[0],a[1],-.12),(b[0],b[1],.12),(a[0],a[1],.12))


def make_model(name):
    mesh = Mesh()
    if name == "title": mesh.japanese_title()
    elif name == "pressSpace": mesh.text("SPACE / A : START", .65)
    elif len(name) == 1 and name in "COURSECLEAR": mesh.text(name, 1.4)
    elif name.startswith("stageLabel"):
        index = int(name[-1]); mesh.text("TUTORIAL" if index == 0 else f"STAGE {index}", .8)
    elif name == "backGuide": mesh.text("ESC / B : BACK", .6)
    elif name in ("F", "radderUI"):
        mesh.text("F" if name == "F" else "W / S", 1)
    elif name == "star": mesh.star()
    elif name in ("soapBubbles", "sun", "moon", "skydome"):
        mesh.sphere(1 if name == "skydome" else .5)
    elif name == "ladder":
        mesh.box((-.35,0,0),(.12,1,.12)); mesh.box((.35,0,0),(.12,1,.12))
        for y in (-.4,-.2,0,.2,.4): mesh.box((0,y,0),(.7,.08,.12))
    elif name == "player":
        mesh.box((0,.15,0),(.45,.7,.3)); mesh.box((0,.65,0),(.35,.35,.35))
        for x in (-.15,.15): mesh.box((x,-.35,0),(.18,.35,.24))
    elif name == "door":
        mesh.box((-.4,0,0),(.16,1,.2)); mesh.box((.4,0,0),(.16,1,.2)); mesh.box((0,.45,0),(.8,.12,.2))
    elif name == "key":
        mesh.box((0,.15,0),(.35,.35,.12)); mesh.box((0,-.2,0),(.1,.4,.12)); mesh.box((.12,-.35,0),(.25,.1,.12))
    elif name == "switch": mesh.box((0,-.3,0),(.8,.25,.8))
    elif name in ("block", "wall", "stageSelect"):
        mesh.box(texture="grass" if name == "block" else name)
    elif name == "midpoint":
        mesh.box((0,0,0),(.1,1,.1)); mesh.box((.2,.3,0),(.4,.3,.06))
    elif name == "spike":
        base=[(-.5,-.5,-.5),(.5,-.5,-.5),(.5,-.5,.5),(-.5,-.5,.5)]
        for i in range(4): mesh.triangle(base[i],(0,.5,0),base[(i+1)%4])
        mesh.triangle(base[0],base[1],base[2]); mesh.triangle(base[0],base[2],base[3])
    elif name in ("terrain", "plane"): mesh.box((0,-.05,0),(1,.1,1))
    elif name == "axis":
        mesh.box((.5,0,0),(1,.08,.08)); mesh.box((0,.5,0),(.08,1,.08)); mesh.box((0,0,.5),(.08,.08,1))
    else: mesh.box()
    return mesh


def targets():
    # MTL-only folders include dynamic editor candidates as well as game models.
    result = {p.with_suffix(".obj") for p in ROOT.rglob("*.mtl")}
    # Sun and moon reused a differently named MTL on the old machine.
    result.update(ROOT/"Models"/name/(name+".obj") for name in ("sun","moon"))
    result.update(ROOT/"Models/Recovery"/(f"stageLabel{i}.obj") for i in range(6))
    result.add(ROOT/"Models/Recovery/backGuide.obj")
    return sorted(result)


def restore_missing_textures(write=False):
    """Reuse surviving source textures without replacing any existing artwork.

    Check every material, including the second material in multiMaterial.mtl.
    Windows absolute paths are resolved by their local basename, like Model.cpp.
    Unknown missing images are errors rather than silently generated white maps.
    """
    sources = {
        "uvChecker.png": ROOT / "Models/axis/uvChecker.png",
        "monsterBall.png": ROOT / "Models/Work/monsterBall.png",
    }
    checked = 0
    restored = []
    for material in sorted(ROOT.rglob("*.mtl")):
        for line in material.read_text(encoding="utf-8-sig").splitlines():
            words = line.strip().split(None, 1)
            if len(words) != 2 or words[0] != "map_Kd":
                continue
            name = words[1].strip().strip('"').replace("\\", "/")
            local = material.parent / Path(name).name
            relative = material.parent / name
            if not relative.is_file() and not local.is_file():
                source = sources.get(local.name)
                if not write or source is None or not source.is_file():
                    raise FileNotFoundError(f"Material texture missing: {material}: {name}")
                shutil.copyfile(source, local)
                restored.append(local)
            checked += 1
    return checked, restored


def validate(path):
    counts = {"v":0,"vt":0,"vn":0,"f":0}
    for line in path.read_text(encoding="utf-8").splitlines():
        tokens = line.split()
        if not tokens: continue
        kind = tokens[0]
        if kind in ("v","vt","vn"):
            assert all(math.isfinite(float(v)) for v in tokens[1:]), path
            counts[kind] += 1
        if kind == "f":
            assert len(tokens) == 4, (path,"Model loader requires triangles")
            for token in tokens[1:]:
                indices = [int(i) for i in token.split("/")]
                assert len(indices) == 3, path
                assert all(1 <= i <= counts[k] for i,k in zip(indices,("v","vt","vn"))), path
            counts["f"] += 1
        if kind == "mtllib":
            assert (path.parent/" ".join(tokens[1:])).is_file(), path
    assert counts["f"] > 0, path
    return counts["f"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write",action="store_true",help="Generate only missing or marked recovery models")
    args = parser.parse_args()
    checked, restored = restore_missing_textures(write=args.write)
    triangles = 0
    for path in targets():
        if args.write and (not path.exists() or path.read_text(encoding="utf-8").startswith(MARKER)):
            mesh = make_model(path.stem)
            material = path.with_suffix(".mtl")
            if material.is_file(): mesh.lines.insert(1,"mtllib " + material.name)
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text("\n".join(mesh.lines)+"\n",encoding="utf-8")
        triangles += validate(path)
    print(f"Validated {len(targets())} recovery/model OBJ files, {triangles} triangles")
    print(f"Material texture references checked: {checked}; restored missing images: {len(restored)}")


if __name__ == "__main__": main()
