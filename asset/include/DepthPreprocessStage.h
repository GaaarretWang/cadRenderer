#ifndef DEPTH_PREPROCESS_STAGE_H
#define DEPTH_PREPROCESS_STAGE_H
#pragma once

#include <vsg/all.h>
#include "FrameImageResources.h"

class DepthPreprocessStage
{
public:
    static constexpr uint32_t iteration_count = 15;

    void build(vsg::ref_ptr<vsg::CommandGraph> command_graph,
               vsg::ref_ptr<vsg::Options> options,
               FrameImageResources& frame_image_resources);

private:
    struct PushConstants
    {
        uint32_t width;
        uint32_t height;
        float valid_depth_threshold;
        uint32_t padding;
    };
};

#endif
