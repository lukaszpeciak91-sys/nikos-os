# Facts source dataset

This directory contains the authoritative editorial source dataset for the
offline `Ciekawostki` / Facts application. Runtime firmware assets are
generated deterministically from these JSONL files; the JSONL remains the source
of truth and is not loaded by the device at runtime.

## Categories and release target

The dataset is split into six JSONL files:

- `space.jsonl`
- `animals.jsonl`
- `technology.jsonl`
- `human.jsonl`
- `history.jsonl`
- `world.jsonl`

The release target is exactly 100 facts per category and 600 facts total.
Normal development validation allows categories to be populated gradually from
0 through 100 records.

## Record format

Each non-empty line is one UTF-8 JSON object:

```json
{"id":"space-001","title":"Doba dłuższa od roku","text":"Na Wenus doba trwa dłużej niż rok.","source":"..."}
```

Required fields are `id`, `title`, `text`, and `source`. The optional `note` field
may be used for editorial context. No other fields are supported in the authoring
format.

The category is inferred from the filename and must not be duplicated in a
record. IDs use the category prefix followed by exactly three decimal digits
and are limited to the numeric range `001` through `100`, for example
`space-001`, `animals-014`, or `technology-087`. IDs must be globally
unique across the full dataset. Development validation accepts any unique
subset of that range; release validation requires the complete `001..100`
set for every category.

`title` is required short Polish editorial metadata (1–48 Unicode characters,
nonblank, trimmed, without control characters). It is stored separately from
the two fact sentences and does not count toward the 180-character `text` limit.
Titles are displayed by Ciekawostki in the category browser and reader.

`source` is required editorial provenance for verification. `source` and
`note` remain authoring-only metadata and are deliberately stripped from
generated firmware assets. Runtime data contains only stable `id`, `title`,
and `text`.

Fact `text` must be natural Polish with correct Polish diacritics and is limited
to 180 Unicode characters.

## Validation

Run development validation while content is incomplete:

```text
python tools/validate_facts.py
```

Run release validation only when all batches are complete:

```text
python tools/validate_facts.py --release
```

Development mode accepts 0..100 facts in each category. Release mode requires
exactly 100 facts in every category and exactly 600 total.

Generate firmware assets:

```text
python tools/generate_facts.py
```

Verify that committed generated assets are reproducible without modifying them:

```text
python tools/generate_facts.py --check
python tools/validate_facts_runtime.py
```

The runtime generator preserves UTF-8 `id/title/text` and never copies
editorial `source/note`.

## Contribution flow

1. Add or edit one category batch.
2. Run the normal validator.
3. Commit the batch.
4. Repeat for the remaining category batches.
5. When every category reaches 100 facts, run `python tools/validate_facts.py --release`.
6. Perform a final editorial audit before merge.
