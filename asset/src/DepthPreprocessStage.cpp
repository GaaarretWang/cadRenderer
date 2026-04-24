#include "DepthPreprocessStage.h"

#include <algorithm>

void DepthPreprocessStage::setParams(const SceneRuntimeState::DepthCompletionParams& params)
{
    params_ = params;
    params_.enable_real_depth_occlusion = params_.enable_real_depth_occlusion != 0 ? 1 : 0;
    params_.shadow_mode = (params_.shadow_mode == SHADOW_REAL_DEPTH) ? SHADOW_REAL_DEPTH : SHADOW_RECEIVER_PLANE;
    params_.valid_depth_min_mm = std::clamp(params_.valid_depth_min_mm, 1, 1000);
    params_.kernel_radius = std::clamp(params_.kernel_radius, 1, 64);
    params_.top_k = std::clamp(params_.top_k, 1, 64);
    params_.spatial_weight = std::clamp(params_.spatial_weight, 0.0f, 1.0f);
    params_.color_sigma = std::clamp(params_.color_sigma, 0.001f, 1.0f);
    params_.edge_threshold = std::clamp(params_.edge_threshold, 0.0f, 1.0f);
    params_.max_fill_passes = std::clamp(params_.max_fill_passes, 1, static_cast<int>(max_dispatch_passes));

    for (auto& push_constant_value : push_constant_values_)
    {
        if (!push_constant_value)
        {
            continue;
        }

        auto& push_constants = push_constant_value->value();
        push_constants.valid_depth_threshold = static_cast<float>(params_.valid_depth_min_mm) / 65535.0f;
        push_constants.kernel_radius = params_.kernel_radius;
        push_constants.top_k = params_.top_k;
        push_constants.spatial_weight = params_.spatial_weight;
        push_constants.color_sigma = params_.color_sigma;
        push_constants.edge_threshold = params_.edge_threshold;
        push_constants.active_pass_count = std::clamp(params_.max_fill_passes, 1, static_cast<int>(max_dispatch_passes));
    }
}

void DepthPreprocessStage::build(vsg::ref_ptr<vsg::CommandGraph> command_graph,
                                 vsg::ref_ptr<vsg::Options> options,
                                 FrameImageResources& frame_image_resources)
{
    vsg::DescriptorSetLayoutBindings descriptor_bindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(descriptor_bindings);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout},
        vsg::PushConstantRanges{{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants)}});

    auto shader_path = vsg::findFile("shaders/output/depth_preprocess.comp", options->paths);
    auto compute_shader = vsg::read_cast<vsg::ShaderStage>(shader_path, options);
    auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_shader);
    command_graph->addChild(vsg::BindComputePipeline::create(pipeline));

    push_constant_values_.clear();
    push_constant_values_.reserve(max_dispatch_passes);

    const auto dispatch_x = (frame_image_resources.width() + 15) / 16;
    const auto dispatch_y = (frame_image_resources.height() + 15) / 16;
    const auto color_info = frame_image_resources.cameraInfo().front();

    auto add_dispatch = [&](uint32_t pass_index,
                            vsg::ref_ptr<vsg::ImageInfo> input_info,
                            vsg::ref_ptr<vsg::ImageInfo> output_info) {
        auto push_constants = vsg::Value<PushConstants>::create(PushConstants{
            frame_image_resources.width(),
            frame_image_resources.height(),
            static_cast<float>(params_.valid_depth_min_mm) / 65535.0f,
            params_.kernel_radius,
            params_.top_k,
            params_.spatial_weight,
            params_.color_sigma,
            params_.edge_threshold,
            std::clamp(params_.max_fill_passes, 1, static_cast<int>(max_dispatch_passes)),
            static_cast<int32_t>(pass_index),
            0,
            0});
        push_constant_values_.push_back(push_constants);

        auto color_descriptor = vsg::DescriptorImage::create(vsg::ImageInfoList{color_info}, 0);
        auto input_descriptor = vsg::DescriptorImage::create(vsg::ImageInfoList{input_info}, 1);
        auto output_descriptor = vsg::DescriptorImage::create(vsg::ImageInfoList{output_info}, 2, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        auto descriptor_set = vsg::DescriptorSet::create(
            descriptor_set_layout,
            vsg::Descriptors{color_descriptor, input_descriptor, output_descriptor});
        command_graph->addChild(vsg::BindDescriptorSet::create(
            VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline_layout,
            descriptor_set));
        command_graph->addChild(vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, push_constants));
        command_graph->addChild(vsg::Dispatch::create(dispatch_x, dispatch_y, 1));
    };

    auto add_image_barrier = [&](vsg::ref_ptr<vsg::Image> image) {
        command_graph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                image,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1})));
    };

    add_dispatch(0, frame_image_resources.rawDepthInputInfo(), frame_image_resources.pingDepthStorageInfo());

    for (uint32_t iteration = 1; iteration < max_dispatch_passes; ++iteration)
    {
        add_image_barrier((iteration % 2 == 1)
                              ? frame_image_resources.pingDepthStorageInfo()->imageView->image
                              : frame_image_resources.pongDepthStorageInfo()->imageView->image);

        add_dispatch(
            iteration,
            (iteration % 2 == 1) ? frame_image_resources.pingDepthInputInfo() : frame_image_resources.pongDepthInputInfo(),
            (iteration % 2 == 1) ? frame_image_resources.pongDepthStorageInfo() : frame_image_resources.pingDepthStorageInfo());
    }

    command_graph->addChild(vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        vsg::ImageMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            frame_image_resources.depthInfo()[0]->imageView->image,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1})));
}
