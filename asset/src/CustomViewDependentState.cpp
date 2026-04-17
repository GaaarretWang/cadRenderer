#include "CustomViewDependentState.h"
#include "OcclusionCullingPasses.h"
#include <algorithm>

using namespace vsg;

namespace
{
constexpr uint32_t kShadowVisibilitySlots = 8;
constexpr uint32_t kShadowPyramidMipCount = 10;

struct ShadowPass1PushConstants
{
    int shadowMapIndex;
    int padding0;
    int padding1;
    int padding2;
};

struct ShadowPass2PushConstants
{
    int width;
    int height;
    int shadowMapIndex;
    int padding;
};

vsg::ref_ptr<vsg::Image> createShadowDepthPyramidImage(uint32_t width, uint32_t height)
{
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32_SFLOAT;
    image->mipLevels = kShadowPyramidMipCount;
    image->arrayLayers = 1;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    image->extent = VkExtent3D{width, height, 1};
    return image;
}

vsg::ref_ptr<vsg::Sampler> createShadowDepthPyramidSampler()
{
    auto sampler = vsg::Sampler::create();
    sampler->minLod = 0;
    sampler->maxLod = static_cast<float>(std::max(0u, kShadowPyramidMipCount - 1));
    sampler->magFilter = VK_FILTER_NEAREST;
    sampler->minFilter = VK_FILTER_NEAREST;
    sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    return sampler;
}

vsg::ref_ptr<vsg::ImageView> createShadowDepthPyramidImageView(vsg::ref_ptr<vsg::Image> image)
{
    auto image_view = vsg::ImageView::create(image);
    image_view->subresourceRange.baseMipLevel = 0;
    image_view->subresourceRange.levelCount = kShadowPyramidMipCount;
    image_view->subresourceRange.baseArrayLayer = 0;
    image_view->subresourceRange.layerCount = 1;
    return image_view;
}

vsg::ref_ptr<vsg::ImageInfo> createShadowDepthLayerImageInfo(vsg::ref_ptr<vsg::Sampler> sampler,
                                                             vsg::ref_ptr<vsg::Image> shadow_depth_image,
                                                             uint32_t layer)
{
    auto depth_image_view = vsg::ImageView::create(shadow_depth_image, VK_IMAGE_ASPECT_DEPTH_BIT);
    depth_image_view->viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_image_view->subresourceRange.baseMipLevel = 0;
    depth_image_view->subresourceRange.levelCount = 1;
    depth_image_view->subresourceRange.baseArrayLayer = layer;
    depth_image_view->subresourceRange.layerCount = 1;
    return vsg::ImageInfo::create(sampler, depth_image_view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

void buildShadowPass1CullGraph(vsg::ref_ptr<vsg::Group> command_graph,
                               vsg::ref_ptr<vsg::Options> options,
                               int shadow_map_index)
{
    vsg::DescriptorSetLayoutBindings descriptor_bindings{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(descriptor_bindings);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout},
        vsg::PushConstantRanges{{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ShadowPass1PushConstants)}});

    auto pre_barrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0);
    command_graph->addChild(pre_barrier);

    auto shader_path = vsg::findFile("shaders/computevertex_shadow_pass1.comp", options->paths);
    auto compute_shader = vsg::read_cast<vsg::ShaderStage>(shader_path, options);
    auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_shader);
    command_graph->addChild(vsg::BindComputePipeline::create(pipeline));

    auto push_constants = vsg::Value<ShadowPass1PushConstants>::create(
        ShadowPass1PushConstants{shadow_map_index, 0, 0, 0});
    command_graph->addChild(vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, push_constants));

    auto post_barrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0);

    for (auto& proto_data_itr : CADMesh::proto_id_to_data_map)
    {
        ProtoData* proto_data = proto_data_itr.second;
        if (proto_data->is_transparent) continue;
        auto storage_buffers = vsg::DescriptorBuffer::create(
            vsg::BufferInfoList{
                proto_data->draw_indirect->bufferInfo,
                proto_data->indirect_full_buffer_info,
                proto_data->input_instance_buffer_info,
                proto_data->input_highlight_buffer_info,
                proto_data->output_instance_buffer_info,
                CADMesh::global_model_matrix_buffer_info,
                CADMesh::last_global_model_matrix_buffer_info,
                proto_data->last_instance_buffer_info,
                proto_data->shadow_visibility_buffer_info},
            0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        auto descriptor_set = vsg::DescriptorSet::create(descriptor_set_layout, vsg::Descriptors{storage_buffers});
        command_graph->addChild(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, descriptor_set));
        command_graph->addChild(vsg::Dispatch::create((static_cast<uint32_t>(proto_data->instance_matrix.size()) + 255u) / 256u, 1, 1));

        pre_barrier->add(vsg::BufferMemoryBarrier::create(
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            proto_data->draw_indirect->bufferInfo->buffer,
            0,
            VK_WHOLE_SIZE));
        pre_barrier->add(vsg::BufferMemoryBarrier::create(
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            proto_data->output_instance_buffer_info->buffer,
            0,
            VK_WHOLE_SIZE));

        post_barrier->add(vsg::BufferMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            proto_data->draw_indirect->bufferInfo->buffer,
            0,
            VK_WHOLE_SIZE));
        post_barrier->add(vsg::BufferMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            proto_data->output_instance_buffer_info->buffer,
            0,
            VK_WHOLE_SIZE));
    }

    command_graph->addChild(post_barrier);
}

void buildShadowDepthPyramidGraph(vsg::ref_ptr<vsg::Group> command_graph,
                                  vsg::ref_ptr<vsg::Options> options,
                                  const CustomViewDependentState::ShadowOcclusionResources& resources,
                                  VkExtent3D extent,
                                  uint32_t layer)
{
    vsg::DescriptorSetLayoutBindings descriptor_bindings{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(descriptor_bindings);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout},
        vsg::PushConstantRanges{{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(OcclusionCullingPasses::ComputePushConstants)}});

    auto transition_shadow_depth = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        resources.shadow_depth_layer_image_info->imageView->image,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, layer, 1});
    command_graph->addChild(vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        transition_shadow_depth));

    {
        auto shader_path = vsg::findFile("shaders/computevertex_depthimage.comp", options->paths);
        auto compute_shader = vsg::read_cast<vsg::ShaderStage>(shader_path, options);
        auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_shader);
        command_graph->addChild(vsg::BindComputePipeline::create(pipeline));
        auto push_constants = vsg::Value<OcclusionCullingPasses::ComputePushConstants>::create(
            OcclusionCullingPasses::ComputePushConstants{extent.width, extent.height, {}});
        command_graph->addChild(vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, push_constants));
        auto descriptor_set = vsg::DescriptorSet::create(
            descriptor_set_layout,
            vsg::Descriptors{
                vsg::DescriptorImage::create(vsg::ImageInfoList{resources.depth_pyramid_image_info, resources.shadow_depth_layer_image_info}, 0)});
        command_graph->addChild(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, descriptor_set));
        command_graph->addChild(vsg::Dispatch::create((extent.width + 31) / 32, (extent.height + 31) / 32, 1));
    }

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
            resources.depth_pyramid_image,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1})));

    for (uint32_t mip_level = 1; mip_level < kShadowPyramidMipCount; ++mip_level)
    {
        auto shader_path = vsg::findFile("shaders/computevertex_depthpyramid.comp", options->paths);
        auto compute_shader = vsg::read_cast<vsg::ShaderStage>(shader_path, options);
        auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_shader);
        command_graph->addChild(vsg::BindComputePipeline::create(pipeline));

        const uint32_t mip_width = std::max(1u, extent.width >> mip_level);
        const uint32_t mip_height = std::max(1u, extent.height >> mip_level);
        auto push_constants = vsg::Value<OcclusionCullingPasses::ComputePushConstants>::create(
            OcclusionCullingPasses::ComputePushConstants{mip_width, mip_height, {}});
        command_graph->addChild(vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, push_constants));

        auto current_image_view = vsg::ImageView::create(resources.depth_pyramid_image);
        current_image_view->subresourceRange.baseMipLevel = mip_level;
        current_image_view->subresourceRange.levelCount = 1;
        current_image_view->subresourceRange.baseArrayLayer = 0;
        current_image_view->subresourceRange.layerCount = 1;
        auto current_image_info = vsg::ImageInfo::create(resources.depth_pyramid_sampler, current_image_view, VK_IMAGE_LAYOUT_GENERAL);

        auto previous_image_view = vsg::ImageView::create(resources.depth_pyramid_image);
        previous_image_view->subresourceRange.baseMipLevel = mip_level - 1;
        previous_image_view->subresourceRange.levelCount = 1;
        previous_image_view->subresourceRange.baseArrayLayer = 0;
        previous_image_view->subresourceRange.layerCount = 1;
        auto previous_image_info = vsg::ImageInfo::create(resources.depth_pyramid_sampler, previous_image_view, VK_IMAGE_LAYOUT_GENERAL);

        auto descriptor_set = vsg::DescriptorSet::create(
            descriptor_set_layout,
            vsg::Descriptors{
                vsg::DescriptorImage::create(vsg::ImageInfoList{current_image_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                vsg::DescriptorImage::create(vsg::ImageInfoList{previous_image_info}, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)});
        command_graph->addChild(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, descriptor_set));
        command_graph->addChild(vsg::Dispatch::create((mip_width + 31) / 32, (mip_height + 31) / 32, 1));

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
                resources.depth_pyramid_image,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, mip_level, 1, 0, 1})));
    }

    command_graph->addChild(vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        vsg::ImageMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resources.depth_pyramid_image,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, kShadowPyramidMipCount, 0, 1})));
}

void buildShadowPass2CullGraph(vsg::ref_ptr<vsg::Group> command_graph,
                               vsg::ref_ptr<vsg::Options> options,
                               const CustomViewDependentState::ShadowOcclusionResources& resources,
                               VkExtent3D extent,
                               int shadow_map_index)
{
    vsg::DescriptorSetLayoutBindings descriptor_bindings{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(descriptor_bindings);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout},
        vsg::PushConstantRanges{{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ShadowPass2PushConstants)}});

    auto pre_barrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0);
    command_graph->addChild(pre_barrier);

    auto shader_path = vsg::findFile("shaders/computevertex_shadow_pass2.comp", options->paths);
    auto compute_shader = vsg::read_cast<vsg::ShaderStage>(shader_path, options);
    auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_shader);
    auto bind_pipeline = vsg::BindComputePipeline::create(pipeline);

    auto seat_shader_path = vsg::findFile("shaders/computevertex_shadow_pass2_seat.comp", options->paths);
    auto compute_shader_seat = vsg::read_cast<vsg::ShaderStage>(seat_shader_path, options);
    auto pipeline_seat = vsg::ComputePipeline::create(pipeline_layout, compute_shader_seat);
    auto bind_pipeline_seat = vsg::BindComputePipeline::create(pipeline_seat);
    command_graph->addChild(bind_pipeline);

    auto push_constants = vsg::Value<ShadowPass2PushConstants>::create(
        ShadowPass2PushConstants{static_cast<int>(extent.width), static_cast<int>(extent.height), shadow_map_index, 0});
    command_graph->addChild(vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, push_constants));

    auto post_barrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0);

    auto current_pipeline = bind_pipeline;
    for (auto& proto_data_itr : CADMesh::proto_id_to_data_map)
    {
        ProtoData* proto_data = proto_data_itr.second;
        if (proto_data->is_transparent) continue;
        const bool use_seat_pipeline = proto_data->instance_matrix.size() > 32;
        if (use_seat_pipeline && current_pipeline == bind_pipeline)
        {
            command_graph->addChild(bind_pipeline_seat);
            current_pipeline = bind_pipeline_seat;
        }
        else if (!use_seat_pipeline && current_pipeline == bind_pipeline_seat)
        {
            command_graph->addChild(bind_pipeline);
            current_pipeline = bind_pipeline;
        }

        auto descriptor_set = vsg::DescriptorSet::create(
            descriptor_set_layout,
            vsg::Descriptors{
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->indirect_full_buffer_info}, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->input_instance_buffer_info}, 2, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->input_highlight_buffer_info}, 3, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->output_instance_buffer_info}, 4, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{resources.camera_plane_info_buffer_info}, 5, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->bounds_buffer_info}, 6, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{resources.camera_matrix_buffer_info}, 7, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorImage::create(vsg::ImageInfoList{resources.depth_pyramid_image_info}, 8),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::global_model_matrix_buffer_info}, 9, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::last_global_model_matrix_buffer_info}, 10, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->last_instance_buffer_info}, 11, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->shadow_visibility_buffer_info}, 12, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)});
        command_graph->addChild(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, descriptor_set));
        if (use_seat_pipeline)
        {
            command_graph->addChild(vsg::Dispatch::create(static_cast<uint32_t>(proto_data->instance_matrix.size() / 700 + 1), 1, 1));
        }
        else
        {
            command_graph->addChild(vsg::Dispatch::create(static_cast<uint32_t>(proto_data->instance_matrix.size() / 32 + 1), 1, 1));
        }

        pre_barrier->add(vsg::BufferMemoryBarrier::create(
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            proto_data->draw_indirect->bufferInfo->buffer,
            0,
            VK_WHOLE_SIZE));
        pre_barrier->add(vsg::BufferMemoryBarrier::create(
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            proto_data->output_instance_buffer_info->buffer,
            0,
            VK_WHOLE_SIZE));

        post_barrier->add(vsg::BufferMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            proto_data->draw_indirect->bufferInfo->buffer,
            0,
            VK_WHOLE_SIZE));
        post_barrier->add(vsg::BufferMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            proto_data->output_instance_buffer_info->buffer,
            0,
            VK_WHOLE_SIZE));
    }

    command_graph->addChild(post_barrier);
}
}

//////////////////////////////////////
//
// TraverseChildrenOfNode
//
namespace vsg
{
    class TraverseChildrenOfNode : public Inherit<Node, TraverseChildrenOfNode>
    {
    public:
        explicit TraverseChildrenOfNode(Node* in_node) :
            node(in_node) {}

        observer_ptr<Node> node;

        template<class N, class V>
        static void t_traverse(N& in_node, V& visitor)
        {
            if (auto ref_node = in_node.node.ref_ptr()) ref_node->traverse(visitor);
        }

        void traverse(Visitor& visitor) override { t_traverse(*this, visitor); }
        void traverse(ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
        void traverse(RecordTraversal& visitor) const override { t_traverse(*this, visitor); }
    };
    VSG_type_name(TraverseChildrenOfNode);

    inline double Cpractical(double n, double f, double i, double m, double lambda)
    {
        double Clog = n * std::pow((f / n), (i / m));
        double Cuniform = n + (f - n) * (i / m);
        return Clog * lambda + Cuniform * (1.0 - lambda);
    };

} // namespace vsg

vsg::ref_ptr<vsg::Image> CustomViewDependentState::createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage){
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{width, height, 1};
    image->mipLevels = 1;
    image->arrayLayers = levels;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    image->flags = 0;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return image;
}

// to override descriptorset layout
void CustomViewDependentState::init(ResourceRequirements& requirements)
{
    // check if ViewDependentState has already been initialized
    if (lightData) return;

    uint32_t maxNumberLights = 64;
    uint32_t maxViewports = 1;

    uint32_t shadowWidth = 2048;
    uint32_t shadowHeight = 2048;
    uint32_t maxShadowMaps = 8;

    auto& viewDetails = requirements.views[view];

    if (view->features != 0)
    {
        uint32_t numLights = static_cast<uint32_t>(viewDetails.lights.size());
        uint32_t numShadowMaps = 0;
        for (auto& light : viewDetails.lights)
        {
            numShadowMaps += light->shadowMaps;
        }

        if (numLights < requirements.numLightsRange[0])
            maxNumberLights = requirements.numLightsRange[0];
        else if (numLights > requirements.numLightsRange[1])
            maxNumberLights = requirements.numLightsRange[1];
        else
            maxNumberLights = numLights;

        if (numShadowMaps < requirements.numShadowMapsRange[0])
            maxShadowMaps = requirements.numShadowMapsRange[0];
        else if (numShadowMaps > requirements.numShadowMapsRange[1])
            maxShadowMaps = requirements.numShadowMapsRange[1];
        else
            maxShadowMaps = numShadowMaps;

        shadowWidth = requirements.shadowMapSize.x;
        shadowHeight = requirements.shadowMapSize.y;
    }
    else
    {
        maxNumberLights = 0;
        maxShadowMaps = 0;
    }

    uint32_t lightDataSize = 4 + maxNumberLights * 16 + maxShadowMaps * 16;

#if 0
    if (active)
    {
        info("void ViewDependentState::init(ResourceRequirements& requirements) view = ", view, ", active = ", active);
        info("    viewDetails.indices.size() = ", viewDetails.indices.size());
        info("    viewDetails.bins.size() = ", viewDetails.bins.size());
        info("    viewDetails.lights.size() = ", viewDetails.lights.size());
        info("    maxViewports = ", maxViewports);
        info("    maxNumberLights = ", maxNumberLights);
        info("    maxShadowMaps = ", maxShadowMaps);
        info("    lightDataSize = ", lightDataSize);
        info("    shadowWidth = ", shadowWidth);
        info("    shadowHeight = ", shadowHeight);
        info("    requirements.numLightsRange = ", requirements.numLightsRange);
        info("    requirements.numShadowMapsRange = ", requirements.numShadowMapsRange);
        info("    requirements.shadowMapSize = ", requirements.shadowMapSize);
    }
#endif

    lightData = vec4Array::create(lightDataSize);
    lightData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    lightDataBufferInfo = BufferInfo::create(lightData.get());

    viewportData = vec4Array::create(maxViewports);
    viewportData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    viewportDataBufferInfo = BufferInfo::create(viewportData.get());

    descriptor = DescriptorBuffer::create(BufferInfoList{lightDataBufferInfo, viewportDataBufferInfo}, 0); // hardwired position for now

    // set up ShadowMaps
    auto shadowMapSampler = Sampler::create();
    auto shadowMapSamplerNoCompare = Sampler::create();
#define HARDWARE_PCF 1
#if HARDWARE_PCF == 1
    shadowMapSampler->minFilter = VK_FILTER_LINEAR;
    shadowMapSampler->magFilter = VK_FILTER_LINEAR;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    shadowMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->compareEnable = VK_TRUE;
    shadowMapSampler->compareOp = VK_COMPARE_OP_GREATER;

    shadowMapSamplerNoCompare->minFilter = VK_FILTER_LINEAR;
    shadowMapSamplerNoCompare->magFilter = VK_FILTER_LINEAR;
    shadowMapSamplerNoCompare->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    shadowMapSamplerNoCompare->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
#else
    shadowMapSampler->minFilter = VK_FILTER_NEAREST;
    shadowMapSampler->magFilter = VK_FILTER_NEAREST;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->compareEnable = VK_TRUE;
    shadowMapSampler->compareOp = VK_COMPARE_OP_GREATER;

    shadowMapSamplerNoCompare->minFilter = VK_FILTER_NEAREST;
    shadowMapSamplerNoCompare->magFilter = VK_FILTER_NEAREST;
    shadowMapSamplerNoCompare->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSamplerNoCompare->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
#endif

    if (maxShadowMaps > 0)
    {
        shadowDepthImage = createCustomShadowImage(shadowWidth, shadowHeight, maxShadowMaps, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        auto depthImageView = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        depthImageView->subresourceRange.baseMipLevel = 0;
        depthImageView->subresourceRange.levelCount = 1;
        depthImageView->subresourceRange.baseArrayLayer = 0;
        depthImageView->subresourceRange.layerCount = maxShadowMaps;

        auto depthImageInfo = ImageInfo::create(shadowMapSampler, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapImages = DescriptorImage::create(ImageInfoList{depthImageInfo}, 2);

        auto depthImageSamplerInfo = ImageInfo::create(shadowMapSamplerNoCompare, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapSamplerImages = DescriptorImage::create(ImageInfoList{depthImageSamplerInfo}, 3);
    }
    else
    {
        //
        // fallback to provide a descriptor image to use when the ViewDependentState shadow map generation is not active
        //
        Data::Properties properties;
        properties.format = VK_FORMAT_D32_SFLOAT;
        properties.imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

        auto shadowMapData = floatArray3D::create(1, 1, 1, 0.0f, properties);
        shadowDepthImage = Image::create(shadowMapData);
        shadowDepthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        auto depthImageView = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        depthImageView->subresourceRange.baseMipLevel = 0;
        depthImageView->subresourceRange.levelCount = 1;
        depthImageView->subresourceRange.baseArrayLayer = 0;
        depthImageView->subresourceRange.layerCount = 1;

        auto depthImageInfo = ImageInfo::create(shadowMapSampler, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapImages = DescriptorImage::create(ImageInfoList{depthImageInfo}, 2);

        auto depthImageSamplerInfo = ImageInfo::create(shadowMapSamplerNoCompare, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapSamplerImages = DescriptorImage::create(ImageInfoList{depthImageSamplerInfo}, 3);
    }

    DescriptorSetLayoutBindings descriptorBindings{
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // lightData
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // viewportData
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map 2D texture array
        VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map 2D texture array
    };

    descriptorSetLayout = DescriptorSetLayout::create(descriptorBindings);
    descriptorSet = DescriptorSet::create(descriptorSetLayout, Descriptors{descriptor, shadowMapImages, shadowMapSamplerImages});

    // if not active then don't enable shadow maps
    if (maxShadowMaps == 0) return;

    // create a switch to toggle on/off the render to texture subgraphs for each shadowmap layer
    shadow_pass_sequence_switch = Switch::create();

    preRenderCommandGraph = CommandGraph::create();
    preRenderCommandGraph->submitOrder = -1;
    preRenderCommandGraph->addChild(shadow_pass_sequence_switch);

    auto tcon = TraverseChildrenOfNode::create(view);

    Mask shadowMask = 0x1; // TODO: do we inherit from main scene? how?

    ref_ptr<View> first_view;
    shadowMaps.resize(maxShadowMaps);
    shadow_occlusion_resources.resize(maxShadowMaps);
    for (auto& shadowMap : shadowMaps)
    {
        if (first_view)
        {
            shadowMap.view = View::create(*first_view);
        }
        else
        {
            first_view = View::create(RECORD_BASE);
            shadowMap.view = first_view;
        }

        shadowMap.view->mask = shadowMask;
        shadowMap.view->camera = Camera::create();
        shadowMap.view->addChild(tcon);

        shadowMap.renderGraph = RenderGraph::create();
        shadowMap.renderGraph->addChild(shadowMap.view);
    }

    auto shadow_depth_sampler = vsg::Sampler::create();
    shadow_depth_sampler->minFilter = VK_FILTER_NEAREST;
    shadow_depth_sampler->magFilter = VK_FILTER_NEAREST;
    shadow_depth_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadow_depth_sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadow_depth_sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadow_depth_sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    for (uint32_t shadow_map_index = 0; shadow_map_index < maxShadowMaps; ++shadow_map_index)
    {
        auto& shadow_resources = shadow_occlusion_resources[shadow_map_index];
        shadow_resources.camera_plane_info = vsg::Array<OcclusionCullingPasses::CameraPlaneInfo>::create(1);
        shadow_resources.camera_plane_info->properties.dataVariance = vsg::DYNAMIC_DATA;
        shadow_resources.camera_plane_info_buffer_info = vsg::BufferInfo::create(shadow_resources.camera_plane_info);
        shadow_resources.camera_matrix = vsg::mat4Array::create(2);
        shadow_resources.camera_matrix->properties.dataVariance = vsg::DYNAMIC_DATA;
        shadow_resources.camera_matrix_buffer_info = vsg::BufferInfo::create(shadow_resources.camera_matrix);
        shadow_resources.depth_pyramid_image = createShadowDepthPyramidImage(shadowWidth, shadowHeight);
        shadow_resources.depth_pyramid_sampler = createShadowDepthPyramidSampler();
        shadow_resources.depth_pyramid_image_view = createShadowDepthPyramidImageView(shadow_resources.depth_pyramid_image);
        shadow_resources.depth_pyramid_image_info = vsg::ImageInfo::create(
            shadow_resources.depth_pyramid_sampler,
            shadow_resources.depth_pyramid_image_view,
            VK_IMAGE_LAYOUT_GENERAL);
        shadow_resources.shadow_depth_layer_image_info =
            createShadowDepthLayerImageInfo(shadow_depth_sampler, shadowDepthImage, shadow_map_index);
        shadow_resources.second_pass_render_graph = RenderGraph::create();
        shadow_resources.second_pass_render_graph->addChild(shadowMaps[shadow_map_index].view);

        auto pass_sequence = vsg::Group::create();
        auto pass1_cull_graph = vsg::Group::create();
        buildShadowPass1CullGraph(pass1_cull_graph, options, static_cast<int>(shadow_map_index));
        pass_sequence->addChild(pass1_cull_graph);

        pass_sequence->addChild(shadowMaps[shadow_map_index].renderGraph);

        auto depth_pyramid_graph = vsg::Group::create();
        buildShadowDepthPyramidGraph(depth_pyramid_graph, options, shadow_resources, shadowDepthImage->extent, shadow_map_index);
        pass_sequence->addChild(depth_pyramid_graph);

        auto pass2_cull_graph = vsg::Group::create();
        buildShadowPass2CullGraph(pass2_cull_graph, options, shadow_resources, shadowDepthImage->extent, static_cast<int>(shadow_map_index));
        pass_sequence->addChild(pass2_cull_graph);
        pass_sequence->addChild(shadow_resources.second_pass_render_graph);
        shadow_pass_sequence_switch->addChild(MASK_DRAW, pass_sequence);
    }
    vsg::ComputeBounds computeSceneBounds_virtual;
    computeSceneBounds_virtual.traversalMask = MASK_PBR_FULL;
    view->accept(computeSceneBounds_virtual);
    //auto ws_bounds = computeFrustumBounds(clip_near_z, clip_far_z, clipToWorld);
    scene_bound_ws_virtual = computeSceneBounds_virtual.bounds;

    vsg::ComputeBounds computeSceneBounds_real;
    computeSceneBounds_real.traversalMask = MASK_SHADOW_RECEIVER;
    view->accept(computeSceneBounds_real);
    //auto ws_bounds = computeFrustumBounds(clip_near_z, clip_far_z, clipToWorld);
    scene_bound_ws_real = computeSceneBounds_real.bounds;


}

void CustomViewDependentState::compile(Context& context)
{
    ViewDependentState::compile(context);

    if (!view->features || !preRenderCommandGraph || shadow_occlusion_resources.empty())
    {
        return;
    }

    auto extent = shadowDepthImage->extent;
    for (uint32_t layer = 0; layer < shadow_occlusion_resources.size(); ++layer)
    {
        auto& shadow_resources = shadow_occlusion_resources[layer];
        if (shadow_resources.depth_pyramid_image)
        {
            shadow_resources.depth_pyramid_image->compile(context);
        }

        if (!shadow_resources.second_pass_render_graph || shadow_resources.second_pass_render_graph->framebuffer)
        {
            continue;
        }

        auto depth_image_view = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
        depth_image_view->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        depth_image_view->subresourceRange.baseMipLevel = 0;
        depth_image_view->subresourceRange.levelCount = 1;
        depth_image_view->subresourceRange.baseArrayLayer = layer;
        depth_image_view->subresourceRange.layerCount = 1;
        depth_image_view->compile(context);

        RenderPass::Attachments attachments(1);
        attachments[0].format = shadowDepthImage->format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        AttachmentReference ignore_color_reference = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
        AttachmentReference depth_reference = {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        RenderPass::Subpasses subpass_description(3);
        for (auto& subpass : subpass_description)
        {
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachments.emplace_back(ignore_color_reference);
            subpass.depthStencilAttachments.emplace_back(depth_reference);
        }

        RenderPass::Dependencies dependencies(2);
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        auto render_pass = RenderPass::create(context.device, attachments, subpass_description, dependencies);
        auto framebuffer = Framebuffer::create(render_pass, ImageViews{depth_image_view}, extent.width, extent.height, 1);
        shadow_resources.second_pass_render_graph->renderArea.offset = VkOffset2D{0, 0};
        shadow_resources.second_pass_render_graph->renderArea.extent = VkExtent2D{extent.width, extent.height};
        shadow_resources.second_pass_render_graph->framebuffer = framebuffer;
        shadow_resources.second_pass_render_graph->clearValues.resize(1);
        shadow_resources.second_pass_render_graph->clearValues[0].depthStencil = VkClearDepthStencilValue{0.0f, 0};
    }
}

// to save viewMatrix and inverseViewMatrix
void CustomViewDependentState::traverse(RecordTraversal& rt) const
{
    if (!view->features) return;
    if (!draw_shadow_light && !draw_shadow_pose) return;

    // Recompute bounds only when the model pose changes.
    if (draw_shadow_pose) {
        vsg::ComputeBounds computeSceneBounds_virtual;
        computeSceneBounds_virtual.traversalMask = MASK_PBR_FULL;
        view->accept(computeSceneBounds_virtual);
        scene_bound_ws_virtual = computeSceneBounds_virtual.bounds;
    }
    // Otherwise reuse the cached scene_bound_ws_virtual value.

    // useful reference : https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
    // PCF filtering : https://github.com/SaschaWillems/Vulkan/issues/231
    // sampler2DArrayShadow
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineDepthStencilStateCreateInfo.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdSetDepthBoundsTestEnable.html
    //
    // Game industry SIGGRAPH presentation
    // https://www.realtimeshadows.com/sites/default/files/Playing%20with%20Real-Time%20Shadows_0.pdf
    //
    // Soft shadows:
    // https://ogldev.org/www/tutorial42/tutorial42.html
    // https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf
    // https://andrew-pham.blog/2019/08/03/percentage-closer-soft-shadows/
    // https://github.com/vsgopenmw-dev/vsgopenmw/blob/master/files/shaders/lib/view/shadow.glsl

    bool requiresPerRenderShadowMaps = false;
    uint32_t shadowMapIndex = 0;
    uint32_t numShadowMaps = static_cast<uint32_t>(shadowMaps.size());
    if (shadow_pass_sequence_switch)
        shadow_pass_sequence_switch->setAllChildren(false);
    else
        numShadowMaps = 0;

    auto computeFrustumBounds = [&](double n, double f, const dmat4& clipToWorld) -> dbox {
        dbox bounds;
        bounds.add(clipToWorld * dvec3(-1.0, -1.0, n));
        bounds.add(clipToWorld * dvec3(-1.0, 1.0, n));
        bounds.add(clipToWorld * dvec3(1.0, -1.0, n));
        bounds.add(clipToWorld * dvec3(1.0, 1.0, n));
        bounds.add(clipToWorld * dvec3(-1.0, -1.0, f));
        bounds.add(clipToWorld * dvec3(-1.0, 1.0, f));
        bounds.add(clipToWorld * dvec3(1.0, -1.0, f));
        bounds.add(clipToWorld * dvec3(1.0, 1.0, f));

        return bounds;
    };

    auto computeLightSpaceBounds = [&](const dbox& wsBound, const dmat4& viewMatrix) -> dbox {
        dbox bounds;
        bounds.add(viewMatrix * dvec3(wsBound.min.x, wsBound.min.y, wsBound.min.z));
        bounds.add(viewMatrix * dvec3(wsBound.min.x, wsBound.min.y, wsBound.max.z));
        bounds.add(viewMatrix * dvec3(wsBound.min.x, wsBound.max.y, wsBound.min.z));
        bounds.add(viewMatrix * dvec3(wsBound.min.x, wsBound.max.y, wsBound.max.z));
        bounds.add(viewMatrix * dvec3(wsBound.max.x, wsBound.min.y, wsBound.min.z));
        bounds.add(viewMatrix * dvec3(wsBound.max.x, wsBound.min.y, wsBound.max.z));
        bounds.add(viewMatrix * dvec3(wsBound.max.x, wsBound.max.y, wsBound.min.z));
        bounds.add(viewMatrix * dvec3(wsBound.max.x, wsBound.max.y, wsBound.max.z));
        return bounds;
    };

    // info("\n\nViewDependentState::traverse(", &rt, ", ", &view, ") numShadowMaps = ", numShadowMaps);

    // set up the light data
    auto light_itr = lightData->begin();
    lightData->dirty();

    (*light_itr++) = vec4(static_cast<float>(ambientLights.size()),
                          static_cast<float>(directionalLights.size()),
                          static_cast<float>(pointLights.size()),
                          static_cast<float>(spotLights.size()));

    // lightData requirements = vec4 * (num_ambientLights + 3 * num_directionLights + 3 * num_pointLights + 4 * num_spotLights + 4 * num_shadow_maps)

    for (auto& entry : ambientLights)
    {
        auto light = entry.second;
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
    }

    for (auto& [mv, light] : directionalLights)
    {
        // info("   light ", light->className(), ", light->shadowMaps = ", light->shadowMaps);

        // assign basic direction light settings to light data
        auto eye_direction = normalize(light->direction * inverse_3x3(mv));
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
        (*light_itr++).set(static_cast<float>(eye_direction.x), static_cast<float>(eye_direction.y), static_cast<float>(eye_direction.z), light->area);

        uint32_t remainingShadowMaps = numShadowMaps - shadowMapIndex;
        uint32_t remainingVisibilitySlots = (shadowMapIndex < kShadowVisibilitySlots) ? (kShadowVisibilitySlots - shadowMapIndex) : 0u;
        uint32_t activeNumShadowMaps = std::min(light->shadowMaps, std::min(remainingShadowMaps, remainingVisibilitySlots));
        (*light_itr++).set(static_cast<float>(activeNumShadowMaps), 0.0f, 0.0f, 0.0f); // shadow map setting

        if (activeNumShadowMaps == 0) continue;

        // set up shadow map rendering backend
        requiresPerRenderShadowMaps = true;

        // compute directional light space
        auto projectionMatrix = view->camera->projectionMatrix->transform();
        auto viewMatrix = view->camera->viewMatrix->transform();
        auto inverse_viewMatrix = inverse(viewMatrix);

        // view direction in world coords
        auto view_direction = normalize(dvec3(0.0, 0.0, -1.0) * (projectionMatrix * viewMatrix));
        auto view_up = normalize(dvec3(0.0000000001, -1.0, 0.0000000001) * (projectionMatrix * viewMatrix));

        // light direction in world coords
        auto light_direction = normalize(light->direction * (inverse_3x3(mv * inverse_viewMatrix)));
#if 0
        info("   directional light : light direction in world = ", light_direction, ", light->shadowMaps = ", light->shadowMaps);
        info("      light->direction in model = ", light->direction);
        info("      view_direction in world = ", view_direction);
        info("      view_up in world = ", view_up);
#endif
        auto light_x_direction = cross(light_direction, view_direction);
        auto light_x_up = cross(light_direction, view_up);

        auto light_x = (length(light_x_direction) > length(light_x_up)) ? normalize(light_x_direction) : normalize(light_x_up);
        auto light_y = cross(light_x, light_direction);
        auto light_z = light_direction;

        auto clipToEye = inverse(projectionMatrix);

        auto n = -(clipToEye * dvec3(0.0, 0.0, 1.0)).z;
        auto f = -(clipToEye * dvec3(0.0, 0.0, 0.0)).z;

        // clamp the near and far values
        if (n > maxShadowDistance)
        {
            // near plane further than maximum shadow distance so no need to generate shadow maps
            continue;
        }
        if (f > maxShadowDistance)
        {
            f = maxShadowDistance;
        }

        auto updateCamera = [&](double clip_near_z, double clip_far_z, const dmat4& clipToWorld) -> void {
            const auto& shadowMap = shadowMaps[shadowMapIndex];
            shadow_pass_sequence_switch->children[shadowMapIndex].mask = MASK_ALL;

            const auto& camera = shadowMap.view->camera;
            auto lookAt = camera->viewMatrix.cast<LookAt>();
            auto ortho = camera->projectionMatrix.cast<Orthographic>();

            if (!lookAt) camera->viewMatrix = lookAt = LookAt::create();
            if (!ortho) camera->projectionMatrix = ortho = Orthographic::create();

            auto ws_bounds = computeFrustumBounds(clip_near_z, clip_far_z, clipToWorld);
            auto sm_eye = (ws_bounds.min + ws_bounds.max) * 0.5 - light_z * (0.5 * length(ws_bounds.max - ws_bounds.min));

            lookAt->eye = sm_eye;
            lookAt->center = sm_eye + light_z;
            lookAt->up = light_y;

            // auto ls_bounds = computeFrustumBounds(clip_near_z, clip_far_z, lookAt->transform() * clipToWorld);
            auto ls_bounds_virtual = computeLightSpaceBounds(scene_bound_ws_virtual, lookAt->transform());
            auto ls_bounds_real = computeLightSpaceBounds(scene_bound_ws_real, lookAt->transform());

            ortho->left = ls_bounds_virtual.min.x;
            ortho->right = ls_bounds_virtual.max.x;
            ortho->bottom = ls_bounds_virtual.min.y;
            ortho->top = ls_bounds_virtual.max.y;
            ortho->nearDistance = -ls_bounds_virtual.max.z;

            // Intersect the eight bounding-box corners with the world-space z = -2 plane and keep the farthest distance.
            double target_world_z = -2.0;
            double max_far_distance = ortho->nearDistance;

            if (std::abs(light_z.z) > 1e-6) {
                // Iterate over the eight corners of the bounding box.
                for (int i = 0; i < 8; ++i) {
                    dvec3 corner(
                        (i & 1) ? ws_bounds.max.x : ws_bounds.min.x,
                        (i & 2) ? ws_bounds.max.y : ws_bounds.min.y,
                        (i & 4) ? ws_bounds.max.z : ws_bounds.min.z
                    );

                    // Compute parameter t from the corner along the light direction toward the target plane.
                    double t = (target_world_z - corner.z) / light_z.z;

                    if (t > 0.0) {
                        // Compute the intersection point in world space.
                        dvec3 intersection_world = corner + light_z * t;

                        // Transform the point into light space.
                        dvec4 intersection_light = lookAt->transform() * dvec4(intersection_world, 1.0);

                        // Update the farthest distance.
                        max_far_distance = std::max(max_far_distance, -intersection_light.z);
                    }
                }
            }

            // Clamp the maximum distance.
            double max_additional_distance = (ls_bounds_virtual.max.z - ls_bounds_virtual.min.z) * 2.0;
            ortho->farDistance = std::min(max_far_distance, ortho->nearDistance + max_additional_distance);

            // Ensure the real scene remains enclosed.
            if(!std::isinf(ls_bounds_real.min.z))
                ortho->farDistance = std::max(-ls_bounds_real.min.z, ortho->farDistance);

            dmat4 shadowMapProjView = camera->projectionMatrix->transform() * camera->viewMatrix->transform();
            // The main shading path feeds world-space positions into the shadow matrix, so the
            // original working chain is world -> light clip -> shadow texcoords, without inverse_viewMatrix.
            dmat4 shadowMapTM = scale(0.5, 0.5, 1.0) * translate(1.0, 1.0, 0.0) * shadowMapProjView;

            auto& shadow_resources = shadow_occlusion_resources[shadowMapIndex];
            OcclusionCullingPasses::CameraPlaneInfo camera_plane_info{};
            camera_plane_info.n[0] = vsg::normalize(vsg::vec4(0.0f, 0.0f, -1.0f, static_cast<float>(-ortho->nearDistance)));
            camera_plane_info.n[1] = vsg::normalize(vsg::vec4(0.0f, 0.0f, 1.0f, static_cast<float>(ortho->farDistance)));
            camera_plane_info.n[2] = vsg::normalize(vsg::vec4(1.0f, 0.0f, 0.0f, static_cast<float>(-ortho->left)));
            camera_plane_info.n[3] = vsg::normalize(vsg::vec4(-1.0f, 0.0f, 0.0f, static_cast<float>(ortho->right)));
            camera_plane_info.n[4] = vsg::normalize(vsg::vec4(0.0f, 1.0f, 0.0f, static_cast<float>(-ortho->bottom)));
            camera_plane_info.n[5] = vsg::normalize(vsg::vec4(0.0f, -1.0f, 0.0f, static_cast<float>(ortho->top)));
            shadow_resources.camera_plane_info->set(0, camera_plane_info);
            shadow_resources.camera_plane_info->dirty();
            shadow_resources.camera_matrix->set(0, vsg::mat4(camera->viewMatrix->transform()));
            shadow_resources.camera_matrix->set(1, vsg::mat4(shadowMapProjView));
            shadow_resources.camera_matrix->dirty();

            // convert tex gen matrix to float matrix and assign to light data
            mat4 m(shadowMapTM);

            (*light_itr++) = m[0];
            (*light_itr++) = m[1];
            (*light_itr++) = m[2];
            (*light_itr++) = m[3];

            // info("m = ", m);

            // advance to the next shadowMap
            shadowMapIndex++;
        };

#if 0
        info("     light_x = ", light_x);
        info("     light_y = ", light_y);
        info("     light_z = ", light_z);
#endif

#if 0
        double range = f - n;
        info("    n = ", n, ", f = ", f, ", range = ", range);
#endif
        auto clipToWorld = inverse(projectionMatrix * viewMatrix);

        if (activeNumShadowMaps > 1)
        {
            double m = static_cast<double>(activeNumShadowMaps);
            for (double i = 0; i < m; i += 1.0)
            {
                dvec3 eye_near(0.0, 0.0, -Cpractical(n, f, i, m, lambda));
                dvec3 eye_far(0.0, 0.0, -Cpractical(n, f, i + 1.0, m, lambda));

                auto clip_near = projectionMatrix * eye_near;
                auto clip_far = projectionMatrix * eye_far;

                updateCamera(clip_near.z, clip_far.z, clipToWorld);
            }
        }
        else
        {
            dvec3 eye_near(0.0, 0.0, -n);
            dvec3 eye_far(0.0, 0.0, -f);

            auto clip_near = projectionMatrix * eye_near;
            auto clip_far = projectionMatrix * eye_far;

            updateCamera(clip_near.z, clip_far.z, clipToWorld);
        }
    }

    for (auto& [mv, light] : pointLights)
    {
        auto eye_position = mv * light->position;
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
        (*light_itr++).set(static_cast<float>(eye_position.x), static_cast<float>(eye_position.y), static_cast<float>(eye_position.z), 0.0f);
    }

    for (auto& [mv, light] : spotLights)
    {
        auto eye_position = mv * light->position;
        auto eye_direction = normalize(light->direction * inverse_3x3(mv));
        float cos_innerAngle = static_cast<float>(cos(light->innerAngle));
        float cos_outerAngle = static_cast<float>(cos(light->outerAngle));
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
        (*light_itr++).set(static_cast<float>(eye_position.x), static_cast<float>(eye_position.y), static_cast<float>(eye_position.z), cos_innerAngle);
        (*light_itr++).set(static_cast<float>(eye_direction.x), static_cast<float>(eye_direction.y), static_cast<float>(eye_direction.z), cos_outerAngle);
    }

    if (requiresPerRenderShadowMaps && preRenderCommandGraph)
    {
        // info("ViewDependentState::traverse(RecordTraversal&) doing pre render command graph. shadowMapIndex = ", shadowMapIndex);
        preRenderCommandGraph->accept(rt);
    }

    draw_shadow_light = false;
    draw_shadow_pose = false;
}
