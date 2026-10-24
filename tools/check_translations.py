#!/usr/bin/env python3
"""Fails when translations/*.json drift from the strings used in the code.

Checks per language: no missing strings, no stale entries, no empty values, and the
same %1..%9 placeholders as the source text.
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from extract_strings import ROOT, extract  # noqa: E402

PLACEHOLDER = re.compile(r"%\d")


def main() -> int:
    sources = extract()
    errors: list[str] = []
    files = sorted((ROOT / "translations").glob("*.json"))
    if not files:
        errors.append("no translation files found")

    for path in files:
        data = json.loads(path.read_text(encoding="utf-8"))
        code = path.stem
        for s in sources:
            if s not in data:
                errors.append(f"[{code}] missing: {s!r}")
            elif not data[s].strip():
                errors.append(f"[{code}] empty: {s!r}")
            elif sorted(PLACEHOLDER.findall(s)) != sorted(PLACEHOLDER.findall(data[s])):
                errors.append(f"[{code}] placeholder mismatch: {s!r} -> {data[s]!r}")
        for key in data:
            if key not in sources:
                errors.append(f"[{code}] stale entry: {key!r}")

    for e in errors:
        print(e)
    print(f"{len(files)} languages, {len(sources)} strings, {len(errors)} problems")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
