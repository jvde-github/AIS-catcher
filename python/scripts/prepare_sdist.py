#!/usr/bin/env python3
"""Copy upstream Source/ files into python/Source/ for sdist packaging.

The sdist build needs the AIS-catcher decoder source inside the package
directory (scikit-build-core won't include files outside ./pyproject.toml's
parent). This script vendors the slice we need; it's invoked before
`python -m build --sdist`.

Editable installs and wheel builds from a git checkout don't need this —
CMakeLists.txt falls back to ../Source/ when python/Source/ is absent.
"""

import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PYTHON_DIR = HERE.parent
REPO_ROOT = PYTHON_DIR.parent
SRC = REPO_ROOT / "Source"
DST = PYTHON_DIR / "Source"

FILES = [
    "Application/AIS-catcher.h",
    "Marine/AIS.h",
    "Marine/AIS.cpp",
    "Marine/Message.h",
    "Marine/Message.cpp",
    "Marine/MessageHistory.h",
    "Marine/NMEA.h",
    "Marine/NMEA.cpp",
    "JSON/JSON.h",
    "JSON/JSON.cpp",
    "JSON/JSONAIS.h",
    "JSON/JSONAIS.cpp",
    "JSON/KeyDefs.h",
    "JSON/Keys.h",
    "JSON/Keys.cpp",
    "JSON/Parser.h",
    "JSON/Parser.cpp",
    "JSON/StringBuilder.h",
    "JSON/StringBuilder.cpp",
    "Library/Common.h",
    "Library/FIFO.h",
    "Library/Logger.h",
    "Library/Logger.cpp",
    "Library/Signals.h",
    "Library/Stream.h",
    "Library/SWAR.h",
    "Library/TCP.h",
    "Library/ZIP.h",
    "Utilities/Convert.h",
    "Utilities/Convert.cpp",
    "Utilities/Helper.h",
    "Utilities/Helper.cpp",
    "Utilities/PackedInt.h",
    "Utilities/Parse.h",
    "Utilities/Parse.cpp",
]


def main():
    if not SRC.exists():
        sys.exit(f"upstream Source/ not found at {SRC}")
    if DST.exists():
        shutil.rmtree(DST)
    for rel in FILES:
        src_path = SRC / rel
        dst_path = DST / rel
        if not src_path.exists():
            sys.exit(f"missing upstream file: {src_path}")
        dst_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src_path, dst_path)
    print(f"vendored {len(FILES)} files: {SRC} -> {DST}")


if __name__ == "__main__":
    main()
