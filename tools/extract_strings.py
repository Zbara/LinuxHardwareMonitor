#!/usr/bin/env python3
"""Lists every translatable source string (qsTr in QML, tr() in C++) found in the project."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
QML = re.compile(r'qsTr\(\s*"((?:[^"\\]|\\.)*)"')
CPP = re.compile(r'\btr\(\s*"((?:[^"\\]|\\.)*)"')


def decode(raw: str) -> str:
    return json.loads('"' + raw + '"')


def extract() -> list[str]:
    found: set[str] = set()
    for path in (ROOT / "qml").rglob("*.qml"):
        found.update(decode(m) for m in QML.findall(path.read_text(encoding="utf-8")))
    for path in (ROOT / "src").rglob("*.cpp"):
        found.update(decode(m) for m in CPP.findall(path.read_text(encoding="utf-8")))
    return sorted(found)


if __name__ == "__main__":
    strings = extract()
    if "--json" in sys.argv:
        print(json.dumps({s: "" for s in strings}, ensure_ascii=False, indent=1))
    else:
        print("\n".join(strings))
        print(f"\n{len(strings)} strings", file=sys.stderr)
