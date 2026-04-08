#pragma once

#include <string>

#include <RenderState.h>

#include "SceneConfigSerializer.h"
#include "SceneFrameProvider.h"

class SceneInitAssembler
{
public:
    static VkSampleCountFlagBits toSampleCount(int msaa);
    static SceneInitPayload buildSceneInitPayload(const SceneConfig& scene_config, const std::string& rendering_dir);
    static CameraFrameInput buildCameraFrameInput(const FrameData& frame_data);
};
