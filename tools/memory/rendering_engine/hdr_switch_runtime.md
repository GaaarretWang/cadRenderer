# HDR Switch Runtime Behavior

## Core Design

HDR switching is designed around stable bindings:

- Initialization binds active empty textures for environment, irradiance, and prefilter data.
- Runtime HDR switching copies precomputed textures into those already-bound active textures.
- Descriptor image handles, pipeline layouts, shaders, and scene graph structure should remain stable during a runtime HDR switch.

## Runtime Rules

- Do not rebuild skybox nodes during a GUI-triggered HDR switch.
- Do not rebuild camera-base nodes during a GUI-triggered HDR switch.
- Do not call `viewer->compile()` during a GUI-triggered HDR switch.
- Do not change descriptor bindings just to switch HDR environments.
- Keep the runtime path focused on GPU texture copy, light state update, and dirtying already-bound dynamic data.

## GUI Record Constraint

HDR buttons are triggered from ImGui `record()`. That call chain must not
synchronously mutate render graph structure, rebuild pipelines, or compile the
viewer.

Safe pattern:

- UI calls should mark an HDR update as pending.
- The renderer should flush the pending update after `viewer->present()`.
- The flush should run the texture copy and lightweight runtime state updates.

## Directional Lights

Directional light switching should not require a compile during HDR switching.
Prefer updating already-created state, masks, or existing groups instead of
adding new uncompiled render resources from the GUI path.

## Regression Context

During bisect, `cd06e69` was observed as good and `7fc772a` as bad for HDR
switch testing. `7fc772a` added a camera image binding to the camera-base path.
The broader runtime rule learned from the regression is that GUI-triggered HDR
switching must not rebuild or compile rendering structures.
