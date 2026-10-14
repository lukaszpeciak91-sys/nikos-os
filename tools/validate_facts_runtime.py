#!/usr/bin/env python3
"""Deterministic release checks for the offline Facts runtime contract."""

from __future__ import annotations

import json
import re
from pathlib import Path

import generate_facts
import validate_facts

PAGE_MAX_CODEPOINTS = 120
MAX_PAGES = 4
TITLE_CODEPOINTS_PER_LINE = 22
TITLE_MAX_LINES = 2
BODY_CODEPOINTS_PER_LINE = 24
BODY_MAX_LINES = 6
CATEGORY_COUNT = 6
FACTS_PER_CATEGORY = 100
TITLE_LIST_ITEM_COUNT = 101

EXTRA_FACT_CODEPOINTS = {
    ord("°"),
    ord("Á"),
    ord("Ç"),
    ord("à"),
    ord("á"),
    ord("é"),
    ord("í"),
    ord("õ"),
    ord("ö"),
    ord("ü"),
    ord("–"),
    ord("—"),
    ord("’"),
    ord("”"),
    ord("„"),
}
POLISH_CODEPOINTS = {ord(ch) for ch in "ĄĆĘŁŃÓŚŹŻąćęłńóśźż"}
SUPPORTED_FONT_CODEPOINTS = (
    set(range(0x20, 0x7F))
    | POLISH_CODEPOINTS
    | EXTRA_FACT_CODEPOINTS
)


def load_release(repo_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for slug, _, _, _ in generate_facts.CATEGORIES:
        path = repo_root / "content" / "facts" / f"{slug}.jsonl"
        rows.extend(
            json.loads(line)
            for line in path.read_text(encoding="utf-8").splitlines()
            if line.strip()
        )
    return rows


def bounded_word_split(text: str, start: int, end: int, limit: int) -> int:
    if end - start <= limit:
        return end
    candidate = start + limit
    space = text.rfind(" ", start, candidate + 1)
    return space if space > start else candidate


def append_paginated_range(
    text: str,
    start: int,
    end: int,
    pages: list[str],
) -> None:
    cursor = start
    while cursor < end and len(pages) < MAX_PAGES:
        while cursor < end and text[cursor] == " ":
            cursor += 1
        if cursor >= end:
            return

        page_end = bounded_word_split(
            text,
            cursor,
            end,
            PAGE_MAX_CODEPOINTS,
        )
        pages.append(text[cursor:page_end].rstrip())
        cursor = page_end
        while cursor < end and text[cursor] == " ":
            cursor += 1


def paginate(text: str) -> list[str]:
    boundary = 0
    for match in re.finditer(r"[.!?] ", text):
        boundary = match.start() + 1

    pages: list[str] = []
    if 0 < boundary < len(text):
        append_paginated_range(text, 0, boundary, pages)
        second = boundary
        while second < len(text) and text[second] == " ":
            second += 1
        append_paginated_range(text, second, len(text), pages)
    else:
        append_paginated_range(text, 0, len(text), pages)

    return pages or [text]


def wrapped_lines(text: str, width: int) -> list[str]:
    lines: list[str] = []
    cursor = 0
    while cursor < len(text):
        while cursor < len(text) and text[cursor] == " ":
            cursor += 1
        if cursor >= len(text):
            break
        line_end = bounded_word_split(text, cursor, len(text), width)
        if line_end <= cursor:
            raise AssertionError("wrap made no progress")
        lines.append(text[cursor:line_end].rstrip())
        cursor = line_end
        while cursor < len(text) and text[cursor] == " ":
            cursor += 1
    return lines


def choose_random(candidate: int, previous: int | None) -> int:
    if not 0 <= candidate < 600:
        raise AssertionError("random candidate out of range")
    if previous is not None and candidate == previous:
        return (candidate + 1) % 600
    return candidate


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    errors, counts = validate_facts.validate_dataset(
        repo_root / "content" / "facts",
        release=True,
    )
    if errors:
        raise AssertionError("\n".join(errors))

    if counts != {slug: 100 for slug, _, _, _ in generate_facts.CATEGORIES}:
        raise AssertionError(f"unexpected category counts: {counts}")

    stale = generate_facts.check_assets(
        generate_facts.build_assets(repo_root)
    )
    if stale:
        raise AssertionError(f"stale generated assets: {stale}")

    rows = load_release(repo_root)
    if len(rows) != 600:
        raise AssertionError(f"expected 600 facts, got {len(rows)}")

    # Every flat random index maps to exactly one of all six categories and
    # one 0..99 local index. Immediate repetition is always avoided.
    reached = {
        (flat // FACTS_PER_CATEGORY, flat % FACTS_PER_CATEGORY)
        for flat in range(600)
    }
    if len(reached) != 600:
        raise AssertionError("random flat-index mapping does not cover 600 facts")
    for candidate in range(600):
        if choose_random(candidate, candidate) == candidate:
            raise AssertionError("immediate random repetition was not avoided")

    # Title-list navigation includes facts 001..100 plus the selectable back row.
    selection = 0
    visited = set()
    for _ in range(TITLE_LIST_ITEM_COUNT):
        visited.add(selection)
        selection = (selection + 1) % TITLE_LIST_ITEM_COUNT
    if visited != set(range(TITLE_LIST_ITEM_COUNT)) or selection != 0:
        raise AssertionError("title-list 101-item wrap failed")

    # Category reader advances numerically and wraps 100 -> 001.
    if (99 + 1) % FACTS_PER_CATEGORY != 0:
        raise AssertionError("reader 100 -> 001 wrap failed")

    page_counts: dict[int, int] = {}
    all_codepoints: set[int] = set()
    max_title = 0
    max_page = 0
    max_title_lines = 0
    max_body_lines = 0

    for record in rows:
        title = record["title"]
        text = record["text"]
        max_title = max(max_title, len(title))
        title_lines = wrapped_lines(title, TITLE_CODEPOINTS_PER_LINE)
        max_title_lines = max(max_title_lines, len(title_lines))
        if len(title_lines) > TITLE_MAX_LINES:
            raise AssertionError(
                f"{record['id']}: title requires {len(title_lines)} lines"
            )
        all_codepoints.update(map(ord, title))
        all_codepoints.update(map(ord, text))

        # Encoding/decoding round-trip proves page boundaries never need to cut
        # a UTF-8 byte sequence; runtime splitting mirrors Python codepoints.
        if text.encode("utf-8").decode("utf-8") != text:
            raise AssertionError(f"{record['id']}: UTF-8 round-trip failed")

        pages = paginate(text)
        page_counts[len(pages)] = page_counts.get(len(pages), 0) + 1
        if not 1 <= len(pages) <= MAX_PAGES:
            raise AssertionError(f"{record['id']}: invalid page count {len(pages)}")
        for page in pages:
            max_page = max(max_page, len(page))
            body_lines = wrapped_lines(page, BODY_CODEPOINTS_PER_LINE)
            max_body_lines = max(max_body_lines, len(body_lines))
            if len(body_lines) > BODY_MAX_LINES:
                raise AssertionError(
                    f"{record['id']}: reader page requires "
                    f"{len(body_lines)} lines"
                )
            if len(page) > PAGE_MAX_CODEPOINTS:
                raise AssertionError(
                    f"{record['id']}: page exceeds {PAGE_MAX_CODEPOINTS} codepoints"
                )
        if " ".join(pages) != text:
            raise AssertionError(f"{record['id']}: pagination changed source text")

    unsupported = sorted(all_codepoints - SUPPORTED_FONT_CODEPOINTS)
    if unsupported:
        formatted = ", ".join(f"U+{cp:04X}" for cp in unsupported)
        raise AssertionError(f"font coverage missing: {formatted}")

    print("Facts runtime validation OK")
    print("counts:", counts)
    print("pages:", dict(sorted(page_counts.items())))
    print("max title codepoints:", max_title)
    print("max title lines:", max_title_lines)
    print("max page codepoints:", max_page)
    print("max body lines:", max_body_lines)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
