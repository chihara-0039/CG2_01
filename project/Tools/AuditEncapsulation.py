"""Review self-authored C++ headers for common encapsulation regressions.

This is a source-pattern aid, not a complete C++ parser or proof of correctness.
Public value structs and engine service handles need manual design review.
"""
from pathlib import Path
import argparse
import json
import re


def inspect_header(path):
    depth, scopes, pending = 0, [], ""
    findings = []
    block_comment = False
    for number, raw in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        line = re.sub(r"/\*.*?\*/", "", raw)
        if block_comment:
            if "*/" not in line:
                continue
            line = line.split("*/", 1)[1]
            block_comment = False
        if "/*" in line:
            line = line.split("/*", 1)[0]
            block_comment = True
        line = line.split("//", 1)[0]
        text = line.strip()
        kind = re.match(r"(class|struct)\s+(\w+)[^{;]*\{", text)
        if kind:
            scopes.append([depth + 1, kind[2], "public" if kind[1] == "struct" else "private", kind[1]])
            pending = ""
        access = re.match(r"(public|private|protected):", text)
        if scopes and depth == scopes[-1][0] and access:
            scopes[-1][2] = access[1]
            pending = ""
        elif scopes and depth == scopes[-1][0]:
            pending += " " + text
            if ";" in text or "{" in text:
                declaration = pending.strip()
                if (scopes[-1][2] == "public" and scopes[-1][3] == "class"
                        and ";" in text and "(" not in pending and declaration.strip("; ")
                        and not declaration.startswith(("using ", "typedef ", "static const", "enum ",
                                                        "friend ", "class ", "struct ", "template", "}"))):
                    findings.append({"line": number, "class": scopes[-1][1], "declaration": declaration})
                pending = ""
        depth += line.count("{") - line.count("}")
        while scopes and depth < scopes[-1][0]:
            scopes.pop()
            pending = ""
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    headers = sorted(p for area in ("Engine", "Game") for p in (repo / "project" / area).rglob("*.h"))
    sources = sorted(p for area in ("Engine", "Game") for p in (repo / "project" / area).rglob("*.cpp"))
    public_data, access_markers, mutable_views = [], [], []
    for path in headers:
        relative = path.relative_to(repo).as_posix()
        public_data.extend({"file": relative, **finding} for finding in inspect_header(path))
        for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            code = line.split("//", 1)[0]
            if re.search(r"\bfriend\b|\bprotected\s*:", code):
                access_markers.append({"file": relative, "line": number, "code": code.strip()})
            if re.search(r"(?:std::(?:vector|array|map|unordered_map)<.*|bool)\s*&\s*\w+\s*\(", code) and "const " not in code:
                mutable_views.append({"file": relative, "line": number, "code": code.strip()})
    report = {
        "scope": "project/Engine and project/Game; excludes third-party libraries, value structs and shaders",
        "limitation": "Pattern scan with manual review; not a complete C++ parser",
        "headers": [p.relative_to(repo).as_posix() for p in headers],
        "cpp_files": [p.relative_to(repo).as_posix() for p in sources],
        "public_mutable_class_data_candidates": public_data,
        "friend_or_protected_candidates": access_markers,
        "mutable_container_or_bool_reference_candidates": mutable_views,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"headers": len(headers), "cpp_files": len(sources), "public_data": len(public_data),
                      "friend_or_protected": len(access_markers), "mutable_views": len(mutable_views)}))
    if args.check and (public_data or access_markers or mutable_views):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
