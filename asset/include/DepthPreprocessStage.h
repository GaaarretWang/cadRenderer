#ifndef DEPTH_PREPROCESS_STAGE_H
#define DEPTH_PREPROCESS_STAGE_H
#pragma once

#include <vsg/all.h>

#include "RenderState.h"
#include "FrameImageResources.h"

class DepthPreprocessStage
{
public:
    static constexpr uint32_t max_dispatch_passes = 15;

    void build(vsg::ref_ptr<vsg::CommandGraph> command_graph,
               vsg::ref_ptr<vsg::Options> options,
               FrameImageResources& frame_image_resources);
    void setParams(const SceneRuntimeState::DepthCompletionParams& params);

private:
    struct PushConstants
    {
        uint32_t width;
        uint32_t height;
        float valid_depth_threshold;
        int32_t kernel_radius;
        int32_t top_k;
        float spatial_weight;
        float color_sigma;
        float edge_threshold;
        int32_t active_pass_count;
        int32_t pass_index;
        int32_t padding0;
        int32_t padding1;
    };

    SceneRuntimeState::DepthCompletionParams params_;
    std::vector<vsg::ref_ptr<vsg::Value<PushConstants>>> push_constant_values_;
};

#endif
