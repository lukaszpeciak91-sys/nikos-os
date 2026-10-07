#!/usr/bin/env python3
"""Validate the authoring-time Facts JSONL source dataset."""

from __future__ import annotations

import argparse
import json
import re
import sys
import unicodedata
from pathlib import Path

CATEGORY_LIMIT = 100
TEXT_MAX_CHARS = 180
CATEGORIES = (
    "space",
    "animals",
    "technology",
    "human",
    "history",
    "world",
)
REQUIRED_FIELDS = {"id", "text", "source"}
OPTIONAL_FIELDS = {"note"}
SUPPORTED_FIELDS = REQUIRED_FIELDS | OPTIONAL_FIELDS


def normalize_text(text: str) -> str:
    return " ".join(text.strip().casefold().split())


def has_control_characters(text: str) -> bool:
    return any(unicodedata.category(char) == "Cc" for char in text)


def format_location(path: Path, line_number: int | None, fact_id: str | None) -> str:
    parts = [str(path)]
    if line_number is not None:
        parts.append(f"line {line_number}")
    if fact_id:
        parts.append(f"id {fact_id!r}")
    return ": ".join(parts)


def validate_dataset(data_dir: Path, release: bool) -> tuple[list[str], dict[str, int]]:
    errors: list[str] = []
    counts = {category: 0 for category in CATEGORIES}
    seen_ids: dict[str, tuple[Path, int]] = {}
    seen_texts: dict[str, tuple[Path, int, str]] = {}

    for category in CATEGORIES:
        path = data_dir / f"{category}.jsonl"
        if not path.is_file():
            errors.append(f"{path}: expected category file is missing")
            continue

        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            errors.append(f"{path}: file is not valid UTF-8 ({exc})")
            continue
        except OSError as exc:
            errors.append(f"{path}: could not read file ({exc})")
            continue

        id_pattern = re.compile(rf"^{re.escape(category)}-\d{{3}}$")

        for line_number, raw_line in enumerate(content.splitlines(), start=1):
            if not raw_line.strip():
                continue

            try:
                record = json.loads(raw_line)
            except json.JSONDecodeError as exc:
                errors.append(
                    f"{format_location(path, line_number, None)}: invalid JSON "
                    f"({exc.msg} at column {exc.colno})"
                )
                continue

            if not isinstance(record, dict):
                errors.append(
                    f"{format_location(path, line_number, None)}: record must be a JSON object"
                )
                continue

            counts[category] += 1

            fact_id = record.get("id")
            display_id = fact_id if isinstance(fact_id, str) and fact_id else None
            location = format_location(path, line_number, display_id)

            extra_fields = sorted(set(record) - SUPPORTED_FIELDS)
            if extra_fields:
                errors.append(
                    f"{location}: unsupported field(s): {', '.join(extra_fields)}"
                )

            missing_fields = sorted(REQUIRED_FIELDS - set(record))
            if missing_fields:
                errors.append(
                    f"{location}: missing required field(s): {', '.join(missing_fields)}"
                )

            for field in ("id", "text", "source"):
                if field in record and not isinstance(record[field], str):
                    errors.append(f"{location}: {field} must be a string")

            if "note" in record and not isinstance(record["note"], str):
                errors.append(f"{location}: note must be a string")

            if isinstance(fact_id, str):
                if fact_id != fact_id.strip():
                    errors.append(f"{location}: id must not have leading/trailing whitespace")
                if not id_pattern.fullmatch(fact_id):
                    errors.append(
                        f"{location}: id must match {category}-NNN with exactly three decimal digits"
                    )
                previous = seen_ids.get(fact_id)
                if previous is not None:
                    previous_path, previous_line = previous
                    errors.append(
                        f"{location}: duplicate id; first seen at "
                        f"{previous_path}: line {previous_line}"
                    )
                else:
                    seen_ids[fact_id] = (path, line_number)

            text = record.get("text")
            if isinstance(text, str):
                if text != text.strip():
                    errors.append(
                        f"{location}: text must not have leading/trailing whitespace"
                    )
                if not text.strip():
                    errors.append(f"{location}: text must not be blank")
                if has_control_characters(text):
                    errors.append(
                        f"{location}: text must not contain newline/control characters"
                    )
                if len(text) > TEXT_MAX_CHARS:
                    errors.append(
                        f"{location}: text is {len(text)} characters; maximum is "
                        f"{TEXT_MAX_CHARS}"
                    )

                normalized = normalize_text(text)
                if normalized:
                    previous = seen_texts.get(normalized)
                    if previous is not None:
                        previous_path, previous_line, previous_id = previous
                        previous_label = (
                            f"id {previous_id!r}" if previous_id else "unknown id"
                        )
                        errors.append(
                            f"{location}: duplicate normalized fact text; first seen at "
                            f"{previous_path}: line {previous_line}: {previous_label}"
                        )
                    else:
                        seen_texts[normalized] = (
                            path,
                            line_number,
                            display_id or "",
                        )

            source = record.get("source")
            if isinstance(source, str):
                if source != source.strip():
                    errors.append(
                        f"{location}: source must not have leading/trailing whitespace"
                    )
                if not source.strip():
                    errors.append(f"{location}: source must not be blank")

        if counts[category] > CATEGORY_LIMIT:
            errors.append(
                f"{path}: category has {counts[category]} facts; maximum is {CATEGORY_LIMIT}"
            )

    if release:
        for category in CATEGORIES:
            count = counts[category]
            if count != CATEGORY_LIMIT:
                errors.append(
                    f"{data_dir / f'{category}.jsonl'}: release requires exactly "
                    f"{CATEGORY_LIMIT} facts; found {count}"
                )

        total = sum(counts.values())
        release_total = CATEGORY_LIMIT * len(CATEGORIES)
        if total != release_total:
            errors.append(
                f"{data_dir}: release requires exactly {release_total} facts total; found {total}"
            )

    return errors, counts


def print_summary(counts: dict[str, int]) -> None:
    width = max(len(category) for category in CATEGORIES)
    for category in CATEGORIES:
        print(f"{category:<{width}}: {counts[category]:>3} / {CATEGORY_LIMIT}")
    print()
    print(
        f"total{' ' * (width - len('total'))}: {sum(counts.values()):>3} / "
        f"{CATEGORY_LIMIT * len(CATEGORIES)}"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate the Facts JSONL source dataset."
    )
    parser.add_argument(
        "--release",
        action="store_true",
        help="require exactly 100 facts in every category and 600 total",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parent.parent
    data_dir = repo_root / "content" / "facts"

    errors, counts = validate_dataset(data_dir, release=args.release)

    if errors:
        print("Facts validation FAILED", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        print()
        print_summary(counts)
        return 1

    print("Facts validation OK")
    print()
    print_summary(counts)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
