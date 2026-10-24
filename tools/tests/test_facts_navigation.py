#!/usr/bin/env python3
"""Compile the real Facts app with board stubs and exercise R2 navigation."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
facts = root / "apps/facts"
generated = facts / "generated"

with tempfile.TemporaryDirectory() as tmp:
    executable = str(Path(tmp) / "facts_navigation")
    command = [
        "g++", "-std=c++17", "-O0", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "tools/tests/font_core_stubs"),
        "-I" + str(root / "tools/tests/coin_flip_stubs"),
        "-I" + str(facts / "include"),
        "-I" + str(facts),
        "-I" + str(root / "components/font_core/include"),
        str(root / "tools/tests/test_facts_navigation.cpp"),
        str(facts / "facts_app.cpp"),
        str(facts / "quiz.cpp"),
        str(generated / "facts_data.cpp"),
        *[str(generated / f"facts_data_{category}.cpp")
          for category in ("space", "animals", "technology",
                           "human", "history", "world")],
        str(generated / "quiz_data.cpp"),
        str(root / "components/font_core/font_core.cpp"),
        "-o", executable,
    ]
    subprocess.run(command, check=True)
    subprocess.run([executable], check=True)
print("Facts R2 C++ navigation and unchanged catalogue/Quiz compilation PASS")
