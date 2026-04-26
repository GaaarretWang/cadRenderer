# Hybrid Deferred Pipeline

## Current Main Path

The renderer is intentionally a hybrid deferred pipeline, not a pure deferred
pipeline yet.

Current high-level order:

1. First depth cull.
2. Main render graph writes GBuffer and shadow history.
3. MSAA path manually resolves depth, mask, normal, world position, and material.
4. Depth pyramid and second cull.
5. Second render graph draws newly visible opaque objects plus transparent, wire, text, and receiver content.
6. SSAO.
7. Deferred opaque lighting.
8. Real scene reconstruction.
9. Fallback composite.
10. Deferred composite.

## Two-Stage Culling Constraint

- The first pass only guarantees objects known visible from the previous frame.
- Camera movement can reveal new opaque objects; those depend on depth-pyramid culling and the second render graph.
- Do not remove `MASK_PBR_FULL` from the second render graph until a replacement consumer is fully implemented and verified.

## GBuffer Contract

`custom_pbr.frag` output locations are fixed:

- `location 0`: color
- `location 1`: normal
- `location 2`: world position
- `location 3`: shadow history
- `location 4`: material
- `location 5`: mask

The render pass attachment order must stay aligned with these locations on both
single-sample and MSAA paths.

## Verification Points

- Scene 0 single-sample material colors must remain correct.
- MSAA opaque edges must not show fallback color leaks.
- Transparent objects must not become opaque.
- Shadow receiver must keep receiver-only semantics.
- Newly exposed objects after camera movement must not disappear.
