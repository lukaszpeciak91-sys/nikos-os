# Repository Agent Rules

Before modifying this repository, read:

- `docs/project/architecture.md`
- `docs/project/decisions.md`
- `docs/project/progress.md`
- `docs/project/workflow.md`

These documents are the authoritative project context for implementation and review work.

## Working rules

- Repository code, comments, commit messages, pull request titles and descriptions, and documentation must be written in professional English.
- Prefer the smallest safe change that satisfies the current task.
- Do not silently redesign the architecture.
- Do not create parallel documentation when an authoritative document already exists.
- Do not introduce abstractions for hypothetical future hardware or features without a current use case.
- If implementation exposes an architectural conflict, stop and report it instead of hiding it behind a workaround.

## Entertainment application safety contract

- Every application launched from Entertainment must pass the shared Entertainment Guard.
- New Entertainment applications must participate in shared admission, expiration, Low Battery Reserve, and lifecycle/preemption cleanup.
- Regression tests must detect missing integration; it must never silently permit unrestricted play.
- Tools, Clock, Communicator, and other non-Entertainment services remain outside this limit.
- Extend the small modular architecture; do not add per-game guards or speculative abstractions.
