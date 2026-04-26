# MSAA Resolve Rules

## Core Rule

Manual GBuffer resolve uses one sample-selection policy for all resolved
attachments:

- Select the sample with maximum depth from the MSAA depth image.
- Use that same sample index for mask, normal, world position, and material.

Do not average one attachment independently while selecting another by depth.
Do not let different GBuffer attachments pick different sample indices.

## Current Consumers

- `gbuffer_resolve_depth.frag` writes the selected depth and selected sample index.
- `gbuffer_resolve_mask.frag`, `gbuffer_resolve_normal.frag`,
  `gbuffer_resolve_world_pos.frag`, and `gbuffer_resolve_material.frag` must use
  the exact same selection function.
- SSAO, real scene, and occlusion culling should prefer the manually resolved
  depth target when MSAA is enabled.

## Shader Maintenance

The shared sample-selection logic should live in a GLSL include. If the rule
changes, update the include rather than editing each resolve shader separately.
