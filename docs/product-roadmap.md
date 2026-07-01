# Product Roadmap

The roadmap is ordered so each milestone is independently useful and testable.
Line counts are never acceptance criteria for an individual change.

## Milestone 1: trustworthy baseline

- Remove the nested repository and local build artifacts from submission state.
- Replace process-global codec state with decode-session ownership.
- Replace ambiguous cache ownership with RAII.
- Add a portable clean build and run all existing tests.
- Add valid per-harness seed corpora.
- Separate vulnerable revisions, raw PoCs, and root-cause patches.

## Milestone 2: bounded streaming foundation

- Memory and file byte streams.
- Checked integer and region arithmetic.
- Incremental buffered readers and writers.
- Codec-independent binary structure inspection.
- Exhaustive truncation and boundary tests.

## Milestone 3: color and compositing

- Color-space and transfer-function APIs.
- Alpha conversion and blend modes.
- Palette construction and quantization.
- Layer masks and compositing trees.
- Animation timeline and frame disposal engine.

## Milestone 4: scalable processing

- Tile storage and memory budgets.
- Halo-aware filter scheduling.
- Deterministic parallel execution.
- Processing graph validation and serialization.
- Incremental graph evaluation and cache invalidation.

## Milestone 5: end-user workflows

- Batch manifests and dry-run validation.
- Structured diagnostics and progress reporting.
- Codec conformance inspection.
- Reproducible export profiles.
- End-to-end examples and operator documentation.

## Definition of done for every milestone

- Public API and intended behavior are documented.
- New implementation is exercised by normal product code.
- Tests include valid, invalid, boundary, and resource-limit cases.
- The clean offline build remains deterministic.
- Fuzz harnesses continue to compile and exercise public behavior.
