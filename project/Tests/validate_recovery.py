"""Validate recovery assets and the portable C++ tests, without claiming DX12 readiness."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

PROJECT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT / "Tools"))
import recover_models


def main():
    triangles = sum(recover_models.validate(p) for p in recover_models.targets())
    references = []
    for directory in ("Game", "Engine"):
        for source in (PROJECT/directory).rglob("*.cpp"):
            text = source.read_text(encoding="utf-8-sig")
            for folder, name in re.findall(r'CreateFromOBJ\([^;]*?"(Resources[^"\n]*)"\s*,\s*"([^"\n]*\.obj)"', text):
                path = PROJECT/folder/name
                assert path.is_file(), (source, path)
                references.append(path)
    for character in set("COURSECLEAR"):
        assert (PROJECT/f"Resources/Models/ClearText/{character}/{character}.obj").is_file()

    sequence = (PROJECT/"Resources/Stages/sequence.txt").read_text().splitlines()
    assert len(sequence) == 6, "Recovery stage labels correspond to the six current sequence entries"
    for name in sequence:
        stage = PROJECT/"Resources/Stages"/name
        lines = stage.read_text().splitlines()
        dimensions = tuple(map(int, lines[0].split()))
        kinds = set()
        for line in lines[1:]:
            words = line.split()
            if len(words) >= 6 and words[0].isdigit():
                x,y,z,kind = map(int, words[:4])
                assert all(0 <= v < size for v,size in zip((x,y,z),dimensions)), (stage,line)
                kinds.add(kind)
        assert {6,7} <= kinds, (stage, "Goal and PlayerStart required")
    for path in (PROJECT/"Resources").rglob("*.gltf"):
        data = json.loads(path.read_text())
        for group in ("buffers", "images"):
            for entry in data.get(group, []):
                uri = entry.get("uri", "")
                assert not uri or uri.startswith("data:") or (path.parent/uri).is_file(), (path,uri)
    ET.parse(PROJECT/"CG2_01.vcxproj")

    for path in recover_models.targets():
        result = subprocess.run(["git", "check-ignore", "--no-index", "-q", str(path)], cwd=PROJECT)
        assert result.returncode == 1, (path, "Model should be trackable")
    for path in ("generated/main.obj", "project/Engine/main.obj", "project/Resources/Debug/main.obj"):
        result = subprocess.run(["git", "check-ignore", "--no-index", "-q", path], cwd=PROJECT.parent)
        assert result.returncode == 0, (path, "Intermediate should be ignored")

    # --write is repeatable and must not alter a recovered asset's bytes.
    before = {p:hashlib.sha256(p.read_bytes()).hexdigest() for p in recover_models.targets()}
    subprocess.run([sys.executable, str(PROJECT/"Tools/recover_models.py"), "--write"], check=True)
    assert before == {p:hashlib.sha256(p.read_bytes()).hexdigest() for p in recover_models.targets()}

    with tempfile.TemporaryDirectory(prefix="cg2-recovery-") as tmp:
        binary = Path(tmp)/"recovery_tests"
        subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-I"+str(PROJECT/"Game/Scene"), "-I"+str(PROJECT/"Engine/Math"),
                        str(PROJECT/"Tests/RecoveryTests.cpp"), str(PROJECT/"Game/Scene/StageSelection.cpp"),
                        str(PROJECT/"Game/Scene/SceneManager.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print(f"Assets: {len(before)} OBJ, {triangles} triangles; {len(references)} literal load calls resolved")
    print(f"Stages: {len(sequence)} files with spawn/goal; glTF dependencies and XML checked")
    print("Gitignore and generator repeatability passed. Windows build/render/input/fade remain unverified.")


if __name__ == "__main__":
    main()
