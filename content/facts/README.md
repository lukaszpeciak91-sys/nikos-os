# Facts source dataset

This directory contains the editorial source dataset for the future offline
`Ciekawostki` / Facts application. It is authoring-time content only; firmware
generation and runtime integration are intentionally outside this foundation.

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
{"id":"space-001","text":"Na Wenus doba trwa dłużej niż rok.","source":"..."}
```

Required fields are `id`, `text`, and `source`. The optional `note` field may be
used for editorial context. No other fields are supported in v0.1.

The category is inferred from the filename and must not be duplicated in a
record. IDs use the category prefix followed by exactly three decimal digits,
for example `space-001`, `animals-014`, or `technology-087`. IDs must be
globally unique across the full dataset.

`source` is required editorial provenance for verification. `source` and
`note` are editorial metadata and may be stripped from generated firmware data
later.

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

## Contribution flow

1. Add or edit one category batch.
2. Run the normal validator.
3. Commit the batch.
4. Repeat for the remaining category batches.
5. When every category reaches 100 facts, run `python tools/validate_facts.py --release`.
6. Perform a final editorial audit before merge.
