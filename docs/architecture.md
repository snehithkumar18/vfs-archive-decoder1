# PixelForge Architecture

PixelForge is a C++17 library for decoding, transforming, analyzing, and
exporting raster images in offline desktop, embedded, and batch-processing
workflows. The command interface is a consumer of the library rather than the
center of the design.

## Existing layers

- **Core image model**: image storage, pixel formats, allocation, transforms.
- **Codecs**: BMP, GIF, PPM, and TGA readers and writers.
- **Processing**: convolution, enhancement, effects, drawing, and histograms.
- **Analysis**: segmentation, edges, quality metrics, and feature extraction.
- **Runtime services**: logging, configuration, metadata, and thread pooling.
- **Interfaces**: the command processor and four fuzzing entry points.

## Planned product subsystems

### Streaming I/O

Bounded readers, writers, byte-order helpers, seekable memory streams, and
incremental decode sessions. Codecs consume this layer instead of performing
ad-hoc pointer arithmetic. Tests cover truncation at every byte boundary.

### Color management

Typed color spaces, transfer functions, matrix profiles, chromatic adaptation,
premultiplied alpha, palette quantization, and deterministic dithering. The
pipeline records conversions so callers can preserve or deliberately replace
source color interpretation.

### Compositing and animation

Layer trees, blend modes, masks, frame timelines, disposal semantics, and
bounded frame caches. This turns GIF animation handling into a reusable product
feature instead of codec-local mutable state.

### Tiled processing

Images larger than a configured memory budget are processed through tiles with
explicit halo regions. Filters declare neighborhood requirements and the
scheduler produces deterministic output regardless of worker count.

### Non-destructive processing graphs

A validated directed acyclic graph describes sources, filters, composites, and
exports. Nodes have typed parameters, stable serialization, provenance, and
incremental invalidation. The CLI and batch runner both use this public API.

### Batch manifests

Local JSON-like manifests describe input sets, pipelines, output naming,
resource limits, and failure policy. Execution is offline and never resolves
network URLs. A dry-run mode validates every path and operation before work.

### Codec conformance tools

Structural inspectors report headers, blocks, frames, palettes, and metadata
without fully rendering an image. Round-trip and differential tests share the
same bounded I/O primitives as production decoders.

## Dependency direction

`interfaces -> workflows -> processing/analysis/codecs -> core/io`

Runtime services may be used by all layers but cannot own image or codec state.
Fuzz harnesses call public interfaces and must contain no vulnerability trigger
logic.

## Ownership rules

- Public owning values use RAII containers or smart pointers.
- Caches state whether they own, share, or observe an image; ownership is never
  inferred from a raw pointer.
- Decoder state belongs to a decode session, not a process-global singleton.
- Input-derived arithmetic is checked before allocation or pointer movement.
- A failed operation leaves its output object unchanged.

## Test boundaries

Each subsystem has unit tests, property-style invariant tests, truncation tests,
and integration tests through a normal public workflow. Sanitizer and fuzz runs
supplement these tests; they do not replace them.
