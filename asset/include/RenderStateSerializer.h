#pragma once

#include "JsonConfigManager.h"
#include "RenderState.h"

class RenderStateSerializer
{
public:
    static void applySceneRenderParams(const SceneRenderParams& params, RenderStateHub& state);
    static SceneRenderParams toSceneRenderParams(const RenderStateHub& state);
};
