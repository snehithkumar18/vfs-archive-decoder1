# Submission Readiness

This document records measurable repository state. It is not a claim that the
repository is ready for submission.

## Baseline audit (2026-07-01)

| Requirement | Baseline | Required work |
| --- | --- | --- |
| Genuine product purpose | PixelForge is an image decoding and processing library | Clarify supported workflows and expose a normal application/library build |
| Meaningful source size | Approximately 22,672 nonblank project source/doc lines after subsystem expansion | Continue growth only through reviewed product features; do not count generated or duplicated code |
| Complete clean build | CMake/NMake build and MSVC smoke script exist | Verify on a separate clean Windows checkout and a Linux compiler environment |
| Deterministic/offline build | Local MSVC build passes without downloads | Pin compiler expectations and prove a network-disabled clean build in CI |
| Fuzz harnesses | BMP, TGA, GIF, CLI, expression, and processing-graph harnesses exist and ClusterFuzzLite build tracks expanded core sources | Add harnesses for manifest, stream, color, and tile subsystems when those APIs stabilize |
| Seed corpus | Valid deterministic seeds exist for BMP, TGA, GIF, CLI, expression, and processing-graph targets | Add semantic seeds for new harnesses as they are introduced |
| PoC provenance | Several raw inputs and generators exist outside tracked source | Recreate each raw PoC from the exact vulnerable revision and record repeated sanitizer results |
| Patch quality | Multiple loose patch files target different revisions | Establish one vulnerable baseline and one patch per independently verified defect |
| Repository hygiene | Nested Git repository, local objects, ZIP, scratch data, and loose patches are present | Remove the tracked nested repository and keep local artifacts outside submission history |
| Authorship/originality | Git history includes explicit benchmark-oriented vulnerability changes | Remove contrived triggers; do not make unsupported claims about human authorship |

## Current verification snapshot

- `tools\build_msvc.bat` passed on 2026-07-01.
- CMake/NMake built the expanded `pixelforge` static library.
- CTest passed 8/8 tests: core, algorithms, byte stream, advanced effects, extra, subsystems, stress, and utilities.
- Expression evaluator implementation and `fuzz_expression` harness were added after the initial subsystem checkpoint.
- Processing graph validation/execution and `fuzz_processing_graph` harness were added after the expression checkpoint.
- Stale benchmark corpora (`fuzz_extract`, `fuzz_mount`, `fuzz_vfs`) were removed from `fuzz/corpus`.
- Known remaining gap: repository is not yet at the 50,000 meaningful LOC target.

## Non-negotiable validation gates

1. A clean checkout builds without network access.
2. Product tests pass independently of fuzzing.
3. Every harness reaches normal public product behavior.
4. Every seed is valid and non-crashing.
5. Every submitted PoC is an exact raw harness input.
6. A PoC produces the same sanitizer class and project stack on repeated runs.
7. The matching root-cause patch makes that PoC clean without weakening checks.
8. No generated filler, duplicate tree, build product, or private local path is tracked.

## Growth policy

The 50,000-line requirement is a product-development milestone, not a padding
target. New code must belong to a documented subsystem, have tests, and be used
by the public library or application. Candidate substantive areas are streaming
image I/O, color management, compositing, tiled processing, animation timelines,
non-destructive processing graphs, batch manifests, and codec conformance tools.
