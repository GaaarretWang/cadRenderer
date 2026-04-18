# Directory Structure - Virtual-Reality Fusion Rendering Engine

This document provides a detailed overview of the project's directory structure. Use this reference to quickly locate files and understand the codebase organization before making modifications.

## Root Directory Structure
```
cadRenderer/
├── CLAUDE.md                 # Development guidelines and rules
├── CMakeLists.txt           # Root CMake configuration
├── README.md                # Project documentation
├── run.sh                   # Build and run script (bash run.sh)
├── config.json              # Configuration file
├── include/                 # Client interface headers
│   ├── RenderingServer.h   # Server interface (AR/MR engine compatible)
│   └── Rendering.h         # Client interface
├── src/                     # Client implementation
│   ├── main.cpp            # Main entry point
│   ├── RenderingServer.cpp # Server implementation
│   └── Rendering.cpp       # Client implementation
├── asset/                   # Core rendering engine
│   ├── CMakeLists.txt      # Engine build configuration
│   ├── Params.json         # Rendering parameters
│   ├── LightInfo.json      # Lighting configuration
│   ├── include/            # Engine headers
│   ├── src/                # Engine implementation
│   └── data/               # Resources (shaders, textures, models)
└── .vscode/                # VS Code settings
    └── settings.json
```

## Asset Directory (Core Engine)
### `asset/include/` - Engine Headers
```
include/
├── vsgRendererServer.h     # Main rendering server class
├── CADMesh.h              # CAD model loading and management
├── IBL.h                  # Image-Based Lighting system
├── CustomViewDependentState.h  # Custom view state for shadows
├── CustomViewDependentState1.h # Extended view state
├── SSAOPass.h             # Screen Space Ambient Occlusion
├── OcclusionCullingPasses.h # Occlusion culling system
├── ConfigShader.h         # Shader configuration utilities
├── NvEncoder.h           # NVIDIA encoder interface
├── NvEncoderCuda.h       # CUDA encoder implementation
├── NvDecoder.h           # NVIDIA decoder interface
├── ImGui.h               # GUI integration
├── MyMask.h              # Rendering mask definitions
├── screenshot.h          # Screenshot functionality
├── convertPng.h          # PNG conversion utilities
├── fixDepth.h            # Depth buffer processing
├── Utils.h               # Utility functions
├── OBJLoader.h           # OBJ file loader
├── PlaneLoader.h         # Plane geometry loader
├── encoder.h             # Encoding interface
├── upscaleImage.h        # Image upscaling utilities
├── imageUtils.h          # Image processing utilities
├── tiny_obj_loader.h     # External OBJ loader
├── json.hpp              # JSON library
├── stb_image.h           # Image loading library
└── renderGeo_generated.h # Generated flatbuffer schemas
```

### `asset/src/` - Engine Implementation
```
src/
├── vsgRendererServer.cpp   # Main rendering server implementation
├── CADMesh.cpp            # CAD model processing
├── IBL.cpp                # IBL system implementation
├── CustomViewDependentState.cpp  # Custom view state
├── CustomViewDependentState1.cpp # Extended view state
├── SSAOPass.cpp           # SSAO implementation
├── OcclusionCullingPasses.cpp # Occlusion culling
├── ConfigShader.cpp       # Shader configuration
├── NvEncoder.cpp         # NVIDIA encoder
├── NvEncoderCuda.cpp     # CUDA encoder implementation
├── NvDecoder.cpp         # NVIDIA decoder
├── ImGui.cpp             # GUI implementation
├── OBJLoader.cpp         # OBJ loader implementation
├── PlaneLoader.cpp       # Plane loader implementation
├── encoder.cpp           # Encoding implementation
└── (other implementation files)
```

### `asset/data/` - Resources
```
data/
├── shaders/              # GLSL shader files
│   ├── IBL/             # Image-Based Lighting shaders
│   │   ├── custom_pbr.frag       # Custom PBR fragment shader
│   │   ├── standard.vert         # Standard vertex shader
│   │   ├── skybox.frag/.vert     # Skybox rendering
│   │   ├── ssao_standalone.frag  # Standalone SSAO generation
│   │   ├── ssao_composite.frag   # SSAO + real-scene composite
│   │   ├── real_scene_standalone.frag # Real-scene color reconstruction
│   │   └── (other IBL shaders)
│   ├── shadow.vert/.frag         # Shadow mapping shaders
│   ├── line.vert/.frag           # Line rendering shaders
│   ├── point.vert/.frag          # Point rendering shaders
│   └── compute*.comp             # Compute shaders
├── textures/             # Texture files
│   ├── *.hdr            # HDR environment maps (1.hdr, 2.hdr, etc.)
│   └── (material textures)      # PBR material textures
├── dataset3/             # Test dataset
│   ├── color/           # Color images
│   ├── depth/           # Depth images
│   └── associations.txt # Data associations
├── cameraPose/          # Camera trajectory data
│   └── vsg_pose.txt    # Camera poses
├── fonts/               # Font files
│   └── times.vsgt      # Text rendering font
├── JsonData/           # JSON configuration data
│   ├── CockpitMaterial.json
│   ├── CockpitAnimationState.json
│   └── CockpitAnimationAction.json
└── (model directories)  # CAD models and assets
```

## Key File Categories

### Interface Files (AR/MR Engine Compatible)
- `include/RenderingServer.h` - **CRITICAL**: Server interface, must maintain compatibility
- `include/Rendering.h` - Client interface
- `src/RenderingServer.cpp` - **CRITICAL**: Server implementation, interface must not change without approval
- `src/Rendering.cpp` - Client implementation

### Core Rendering Engine
- `asset/include/vsgRendererServer.h` - Main rendering server class
- `asset/src/vsgRendererServer.cpp` - Main rendering implementation
- `asset/include/CADMesh.h` - CAD model management
- `asset/include/IBL.h` - Environment lighting system

### Shader Files
- `asset/data/shaders/IBL/custom_pbr.frag` - Main PBR fragment shader
- `asset/data/shaders/IBL/standard.vert` - Standard vertex shader
- `asset/data/shaders/shadow.*` - Shadow mapping shaders
- `asset/data/shaders/line.*` - Line rendering shaders

### Configuration Files
- `asset/Params.json` - Rendering parameters
- `asset/LightInfo.json` - Lighting configuration
- `config.json` - Global configuration
- `CMakeLists.txt` - Build configuration (root and asset/)

### Build and Run
- `run.sh` - Build and execute script: `bash run.sh`
- `CMakeLists.txt` - Root and asset/ CMake files

## Important Notes

1. **Interface Protection**: Files in `include/` and `src/` prefixed with `RenderingServer` are critical for AR/MR engine compatibility.

2. **Shader Organization**: Shaders are organized by feature (IBL, shadow, line, point, compute).

3. **Resource Locations**:
   - HDR environment maps: `asset/data/textures/*.hdr`
   - Test data: `asset/data/dataset3/`
   - Camera poses: `asset/data/cameraPose/vsg_pose.txt`

4. **Build Process**: Always use `bash run.sh` to ensure consistent build and execution.

5. **Third-party Dependencies**: Located in `asset/thirdParty/` (excluded from this listing for clarity).

6. **External Code Reference**: When referencing Unity or other external code, use this directory structure to understand the corresponding project organization and locate equivalent files.

7. **Usage Rule**: This file is referenced in CLAUDE.md Rule #10. Always consult this directory structure before making modifications to reduce context switching.

## Quick Reference - Common Modification Paths

### To modify rendering effects:
1. Check shaders in `asset/data/shaders/`
2. Review parameters in `asset/Params.json`
3. Update lighting in `asset/LightInfo.json`

### To modify CAD model handling:
1. Check `asset/include/CADMesh.h` and `asset/src/CADMesh.cpp`
2. Review model loading in OBJLoader/PlaneLoader

### To modify IBL/lighting:
1. Check `asset/include/IBL.h` and `asset/src/IBL.cpp`
2. Review shaders in `asset/data/shaders/IBL/`

### To modify interface (with caution!):
1. Check `include/RenderingServer.h` for interface definitions
2. Ensure AR/MR engine compatibility before any changes

---

*Last Updated: 2026-03-07*
*Use this file as a reference before making modifications to reduce context switching.*
