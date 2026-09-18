# Project Workflow

## Roles

### Architect

- Owns architecture, module boundaries, and expensive-to-reverse design decisions.
- Resolves architectural questions exposed by implementation or review.

### Implementer

- Receives scoped implementation tasks.
- Works in small branches and pull requests.
- Does not silently redesign architecture.

### Auditor

- Independently reviews the actual diff or pull request.
- Checks correctness, coupling, technical debt, concurrency, memory use, warnings, and architecture compliance.
- Does not invent unrelated new features.

### Orchestrator

- Coordinates the overall project workflow and sequencing.
- Keeps work aligned with the current milestone and approved scope.

## Expected iteration

```text
Architect / Orchestrator
-> scoped task
-> Implementer branch + PR
-> checks/build evidence where applicable
-> Auditor review
-> focused fixes on the same branch if required
-> audit again if necessary
-> merge
-> update project progress
-> next scoped task
```

## Rules

- Keep pull requests small and purpose-specific.
- Do not stack unrelated speculative fixes.
- Do not expand scope silently.
- Send architectural disagreements back to the Architect instead of solving them through hidden implementation workarounds.
- Update documentation only when the authoritative project state actually changes.
