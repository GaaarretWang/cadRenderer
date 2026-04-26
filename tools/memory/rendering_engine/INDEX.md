# Rendering Engine Memory Index

Project-specific rendering engine notes live in this directory.

## Runtime HDR And IBL

- [HDR switch runtime behavior](hdr_switch_runtime.md): fixed-descriptor HDR switching design, GUI record constraints, and the no-rebuild/no-compile rule for runtime environment changes.

## Hybrid Deferred Pipeline

- [Hybrid deferred pipeline](hybrid_deferred_pipeline.md): current render graph order, two-stage culling constraints, and protected masks.
- [MSAA resolve rules](msaa_resolve.md): max-depth sample selection and attachment coherence rules for MSAA GBuffer resolve.
- [Composite responsibilities](composite_responsibilities.md): ownership boundaries between deferred opaque, fallback composite, real scene, and deferred composite.

## Update Rules

- Add new findings as separate topic files.
- Keep this index sorted by rendering subsystem.
- Prefer concrete file paths, call chains, and regression commits over vague summaries.
