#include "DepthPreprocessStage.h"

void DepthPreprocessStage::build(vsg::ref_ptr<vsg::CommandGraph> command_graph,
                                 vsg::ref_ptr<vsg::Options> options,
                                 FrameImageResources& frame_image_resources)
{
    vsg::DescriptorSetLayoutBindings descriptor_bindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(descriptor_bindings);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout},
        vsg::PushConstantRanges{{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants)}});

    auto shader_path = vsg::findFile("shaders/depth_preprocess.comp", options->paths);
    auto compute_shader = vsg::read_cast<vsg::ShaderStage>(shader_path, options);
    auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_shader);
    command_graph->addChild(vsg::BindComputePipeline::create(pipeline));

    auto push_constants = vsg::Value<PushConstants>::create(PushConstants{
        frame_image_resources.width(),
        frame_image_resources.height(),
        100.0f / 65535.0f,
        0});
    command_graph->addChild(vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, push_constants));

    auto dispatch_x = (frame_image_resources.width() + 15) / 16;
    auto dispatch_y = (frame_image_resources.height() + 15) / 16;

    auto add_dispatch = [&](vsg::ref_ptr<vsg::ImageInfo> input_info, vsg::ref_ptr<vsg::ImageInfo> output_info) {
        auto input_descriptor = vsg::DescriptorImage::create(vsg::ImageInfoList{input_info}, 0);
        auto output_descriptor = vsg::DescriptorImage::create(vsg::ImageInfoList{output_info}, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        auto descriptor_set = vsg::DescriptorSet::create(descriptor_set_layout, vsg::Descriptors{input_descriptor, output_descriptor});
        command_graph->addChild(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, descriptor_set));
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

    add_dispatch(frame_image_resources.rawDepthInputInfo(), frame_image_resources.pingDepthStorageInfo());

    for (uint32_t iteration = 1; iteration < iteration_count; ++iteration)
    {
        add_image_barrier((iteration % 2 == 1)
                              ? frame_image_resources.pingDepthStorageInfo()->imageView->image
                              : frame_image_resources.pongDepthStorageInfo()->imageView->image);

        add_dispatch(
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
