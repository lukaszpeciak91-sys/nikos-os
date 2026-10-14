#!/usr/bin/env python3
"""Generate deterministic firmware Facts assets from approved JSONL sources."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

CATEGORIES = (
    ("space", "Space", "Space", "KOSMOS"),
    ("animals", "Animals", "Animals", "ZWIERZĘTA"),
    ("technology", "Technology", "Technology", "TECHNOLOGIA"),
    ("human", "Human", "Human", "CZŁOWIEK"),
    ("history", "History", "History", "HISTORIA"),
    ("world", "World", "World", "ŚWIAT"),
)
FACTS_PER_CATEGORY = 100


def cpp_string(value: str) -> str:
    # JSON string escaping is compatible with the C++ UTF-8 literals used here.
    return "u8" + json.dumps(value, ensure_ascii=False)


def load_category(data_dir: Path, slug: str) -> list[dict[str, str]]:
    path = data_dir / f"{slug}.jsonl"
    records = [
        json.loads(line)
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    records.sort(key=lambda record: record["id"])

    if len(records) != FACTS_PER_CATEGORY:
        raise ValueError(
            f"{path}: expected {FACTS_PER_CATEGORY} facts, found {len(records)}"
        )

    for index, record in enumerate(records, start=1):
        expected_id = f"{slug}-{index:03d}"
        if record.get("id") != expected_id:
            raise ValueError(
                f"{path}: expected id {expected_id}, found {record.get('id')!r}"
            )
        if not isinstance(record.get("title"), str):
            raise ValueError(f"{path}: {expected_id} has no string title")
        if not isinstance(record.get("text"), str):
            raise ValueError(f"{path}: {expected_id} has no string text")

    return records


def generated_header() -> str:
    externs = "\n".join(
        f"extern const Fact k{symbol}Facts[kFactsPerCategory];"
        for _, _, symbol, _ in CATEGORIES
    )
    return f"""#pragma once

#include <cstddef>
#include <cstdint>

namespace nikos::facts::data {{

enum class Category : std::uint8_t {{
    Space,
    Animals,
    Technology,
    Human,
    History,
    World,
}};

struct Fact {{
    const char* id;
    const char* title;
    const char* text;
}};

inline constexpr std::size_t kCategoryCount = {len(CATEGORIES)};
inline constexpr std::size_t kFactsPerCategory = {FACTS_PER_CATEGORY};
inline constexpr std::size_t kTotalFacts =
    kCategoryCount * kFactsPerCategory;

{externs}

const Fact* category_facts(Category category);
const char* category_label(Category category);

}}  // namespace nikos::facts::data
"""


def generated_core() -> str:
    cases = "\n".join(
        f"        case Category::{enum_name}:\n"
        f"            return k{symbol}Facts;"
        for _, enum_name, symbol, _ in CATEGORIES
    )
    labels = "\n".join(
        f"        case Category::{enum_name}:\n"
        f"            return {cpp_string(label)};"
        for _, enum_name, _, label in CATEGORIES
    )
    return f"""#include "facts/generated/facts_data.hpp"

namespace nikos::facts::data {{

const Fact* category_facts(Category category)
{{
    switch (category) {{
{cases}
        default:
            return kSpaceFacts;
    }}
}}

const char* category_label(Category category)
{{
    switch (category) {{
{labels}
        default:
            return "KOSMOS";
    }}
}}

}}  // namespace nikos::facts::data
"""


def generated_category(
    slug: str,
    symbol: str,
    records: list[dict[str, str]],
) -> str:
    rows = []
    for record in records:
        rows.append(
            "    {"
            + cpp_string(record["id"])
            + ", "
            + cpp_string(record["title"])
            + ", "
            + cpp_string(record["text"])
            + "},"
        )
    body = "\n".join(rows)
    return f"""#include "facts/generated/facts_data.hpp"

namespace nikos::facts::data {{

const Fact k{symbol}Facts[kFactsPerCategory] = {{
{body}
}};

}}  // namespace nikos::facts::data
"""


def write_if_changed(path: Path, content: str) -> None:
    current = path.read_text(encoding="utf-8") if path.exists() else None
    if current != content:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8", newline="\n")


def generate(repo_root: Path) -> None:
    data_dir = repo_root / "content" / "facts"
    output_dir = repo_root / "apps" / "facts" / "generated"

    loaded = {
        slug: load_category(data_dir, slug)
        for slug, _, _, _ in CATEGORIES
    }
    if sum(len(records) for records in loaded.values()) != 600:
        raise ValueError("release Facts generation requires exactly 600 facts")

    write_if_changed(output_dir / "facts_data.hpp", generated_header())
    write_if_changed(output_dir / "facts_data.cpp", generated_core())

    for slug, _, symbol, _ in CATEGORIES:
        write_if_changed(
            output_dir / f"facts_data_{slug}.cpp",
            generated_category(slug, symbol, loaded[slug]),
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail if committed generated assets differ from regenerated output",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent

    if not args.check:
        generate(repo_root)
        return 0

    output_dir = repo_root / "apps" / "facts" / "generated"
    before = {
        path: path.read_bytes()
        for path in output_dir.glob("facts_data*")
        if path.is_file()
    }
    generate(repo_root)
    after = {
        path: path.read_bytes()
        for path in output_dir.glob("facts_data*")
        if path.is_file()
    }
    if before != after:
        raise SystemExit("generated Facts assets are stale")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
