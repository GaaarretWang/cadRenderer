# Composite Responsibilities

## Ownership Boundaries

- `deferredOpaque`: final lighting for ordinary virtual opaque geometry.
- `realScene`: camera image reconstruction, real depth occlusion, and real-scene shadow.
- `fallback composite`: shadow receiver, real scene fallback, transparent, wire, and text.
- `deferredComposite`: overlays deferred opaque output onto the fallback result.

## Mask Contract

- `mask == 0`: non-opaque fallback, empty, transparent, wire, or text.
- `mask == 1`: virtual opaque.
- `mask == 2`: shadow receiver.

Fallback composite must output transparent black for `mask == 1`; deferred opaque
owns ordinary virtual opaque shading.

## Maintenance Rule

Keep single-sample and MSAA fallback composite branch semantics aligned. MSAA can
still average color samples, but classification must not contradict the mask
contract.
